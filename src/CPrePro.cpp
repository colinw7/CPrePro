#include <CPrePro.h>
#include <CExpr.h>
#include <CFile.h>
#include <CStrUtil.h>
#include <cstring>

#define CPP_SUPPORT 1

#define XSTR(s) STR(s)
#define STR(s) #s

class DefinedFunction : public CExprFunctionObj {
 public:
  DefinedFunction(CPrePro *prepro) :
   prepro_(prepro) {
  }

  CExprValuePtr operator()(CExpr *, const CExprValueArray &values) {
    return prepro_->defined_proc(&values[0], values.size());
  }

 private:
  CPrePro *prepro_ { nullptr };
};

extern int
main(int argc, char **argv)
{
  CPrePro prepro;

  prepro.initialize();

  prepro.process_args(argc, argv);

  prepro.process_files();

  prepro.terminate();

  return 0;
}

CPrePro::
CPrePro()
{
  expr_ = new CExpr;

  expr_->setQuiet(true);

  expr_->addFunction("defined", "nis", new DefinedFunction(this));

  context_stack_.clear();

#ifdef CPRE_PRO_STD_DIRS
  std::string paths_str = XSTR(CPRE_PRO_STD_DIRS);

  std::vector<std::string> paths;

  CStrUtil::addWords(paths_str, paths);

  for (auto &path : paths)
    add_include_dir(path, true);
#endif
}

CPrePro::
~CPrePro()
{
  delete expr_;
}

void
CPrePro::
initialize()
{
  start_context(true);
}

void
CPrePro::
process_args(int argc, char **argv)
{
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (argv[i][1] != '\0')
        process_option(&argv[i][1], i, argv);
    }
    else
      process_arg(argv[i]);
  }
}

void
CPrePro::
process_option(const std::string &option, int &argc, char **argv)
{
  if      (option[0] == 'D')
    add_define_option(option.substr(1));
  else if (option[0] == 'I')
    add_include_option(option.substr(1));
  else if (option[0] == 'o') {
    ++argc;

    output_file_ = argv[argc];
  }
  else if (option == "stdin")
    add_file(nullptr);
  else if (option == "no_blank_lines")
    no_blank_lines_ = true;
  else if (option == "echo")
    echo_input_ = true;
  else if (option == "skip_std")
    skip_std_ = true;
  else if (option == "quiet")
    quiet_ = true;
  else if (option == "debug")
    debug_ = true;
  else if (option == "list_includes")
    list_includes_ = true;
  else
    std::cerr << "Invalid Option " << option << "\n";
}

void
CPrePro::
add_define_option(const std::string &name)
{
  std::string::size_type p = name.find('=');

  VariableList variables;

  if (p != std::string::npos) {
    std::string name1  = name.substr(0, p);
    std::string value1 = name.substr(p + 1);

    add_define(name1, variables, value1);
  }
  else
    add_define(name, variables, "1");
}

void
CPrePro::
add_include_option(const std::string &str)
{
  add_include_dir(str);
}

void
CPrePro::
process_arg(const std::string &arg)
{
  add_file(arg);
}

void
CPrePro::
process_files()
{
  int num_files = files_.size();

  if (num_files == 0)
    add_file("");

  for (int i = 0; i < num_files; i++)
    process_file(files_[i]);
}

void
CPrePro::
process_file(const std::string &fileName)
{
  std::string save_current_file = current_file_;
  uint        save_current_line = current_line_;

  if (fileName != "")
    current_file_ = fileName;
  else
    current_file_ = "<stdin>";

  current_line_ = 0;

  if (debug_)
    std::cerr << "Processing file " << current_file_ << "\n";

  std::vector<std::string> lines;

  if (fileName != "") {
    CFile file(fileName);

    file.toLines(lines);
  }
  else {
    CFile file(stdin);

    file.toLines(lines);
  }

  std::string line1 = "";

  int num_lines = lines.size();

  for (int i = 0; i < num_lines; ++i) {
    std::string line1 = replace_trigraphs(lines[i]);

    int len = line1.size();

    if (len > 0 && line1[len - 1] == '\\') {
      if (echo_input_)
        std::cerr << line1 << "\n";

      std::string line2 = line1.substr(0, len - 1);

      ++i;

      line1 = replace_trigraphs(lines[i]);

      len = line1.size();

      while (i < num_lines && len > 0 && line1[len - 1] == '\\') {
        if (echo_input_)
          std::cerr << line1 << "\n";

        line2 += line1.substr(0, len - 1);

        ++i;

        line1 = replace_trigraphs(lines[i]);

        len = line1.size();
      }

      if (i < num_lines) {
        if (echo_input_)
          std::cerr << line1 << "\n";

        line2 += line1;
      }
      else
        i--;

      current_line_ = i + 1;

      if (! in_comment_ && line2[0] == '#')
        process_line(line2);
      else
        output_line(line2);
    }
    else {
      current_line_ = i + 1;

      if (echo_input_)
        std::cerr << line1 << "\n";

      if (! in_comment_ && line1[0] == '#')
        process_line(line1);
      else
        output_line(line1);
    }
  }

  current_file_ = save_current_file;
  current_line_ = save_current_line;
}

void
CPrePro::
process_line(const std::string &line)
{
  static std::string command;

  std::string line1 = remove_comments(line, true);

  int pos = 1;

  CStrUtil::skipSpace(line1, &pos);

  int pos1 = pos;

  CStrUtil::skipNonSpace(line1, &pos);

  command = line1.substr(pos1, pos - pos1);

  CStrUtil::skipSpace(line1, &pos);

  if (command != "")
    process_command(command, line1.substr(pos));
}

void
CPrePro::
process_command(const std::string &command, const std::string &data)
{
  if      (command == "if"     )
    process_if_command     (data);
  else if (command == "ifdef"  )
    process_ifdef_command  (data);
  else if (command == "ifndef" )
    process_ifndef_command (data);
  else if (command == "else"   )
    process_else_command   (data);
  else if (command == "elif"   )
    process_elif_command   (data);
  else if (command == "endif"  )
    process_endif_command  (data);
  else if (command == "define" )
    process_define_command (data);
  else if (command == "undef"  )
    process_undef_command  (data);
  else if (command == "include")
    process_include_command(data);
  else if (command == "error"  )
    process_error_command  (data);
  else if (command == "warning")
    process_warning_command(data);
  else if (command == "pragma" )
    ;
  else
    std::cerr << "Command '" << command << "' not supported - " <<
                 current_file_ << ":" << current_line_ << "\n";
}

void
CPrePro::
process_if_command(const std::string &data)
{
  start_context(false);

  if (context_->active)
    context_->processing = process_expression(data);
  else
    context_->processing = false;

  context_->processed = context_->processing;
}

void
CPrePro::
process_ifdef_command(const std::string &data)
{
  start_context(false);

  if (context_->active) {
    Define *define = get_define(data);

    context_->processing = (define != nullptr);
  }
  else
    context_->processing = false;

  context_->processed = context_->processing;
}

void
CPrePro::
process_ifndef_command(const std::string &data)
{
  start_context(false);

  if (context_->active) {
    Define *define = get_define(data);

    context_->processing = (define == nullptr);
  }
  else
    context_->processing = false;

  context_->processed = context_->processing;
}

void
CPrePro::
process_else_command(const std::string &)
{
  if (context_->active) {
    context_->processing = ! context_->processed;

    context_->processed = true;
  }
  else
    context_->processing = false;
}

void
CPrePro::
process_elif_command(const std::string &data)
{
  if (context_->active) {
    bool flag = process_expression(data);

    if (flag)
      context_->processing = ! context_->processed;

    if (context_->processing)
      context_->processed = true;
  }
  else
    context_->processing = false;
}

void
CPrePro::
process_endif_command(const std::string &)
{
  if (! end_context())
    std::cerr << "if/endif mismatch - " <<
                 current_file_ << ":" << current_line_ << "\n";
}

void
CPrePro::
process_define_command(const std::string &data)
{
  if (! context_->active || ! context_->processing)
    return;

  int pos = 0;
  int len = data.size();

  CStrUtil::skipSpace(data, &pos);

  int pos1 = pos;

  if (pos < len && (isalpha(data[pos]) || data[pos] == '_')) {
    ++pos;

    while (pos < len && (isalnum(data[pos]) || data[pos] == '_'))
      ++pos;
  }

  if (pos == pos1) {
    std::cerr << "Invalid define '" << data << "' - " <<
                 current_file_ << ":" << current_line_ << "\n";
    return;
  }

  std::string name = data.substr(pos1, pos - pos1);

  VariableList variables;

  if (pos < len && data[pos] == '(') {
    ++pos;

    CStrUtil::skipSpace(data, &pos);

    if (pos < len && data[pos] != ')') {
      pos1 = pos;

      if (pos < len && (isalpha(data[pos]) || data[pos] == '_')) {
        ++pos;

        while (pos < len && (isalnum(data[pos]) || data[pos] == '_'))
          ++pos;
      }

      if (pos == pos1) {
        std::cerr << "Invalid define '" << data << "' - " <<
                     current_file_ << ":" << current_line_ << "\n";
        return;
      }

      std::string variable = data.substr(pos1, pos - pos1);

      variables.push_back(variable);

      CStrUtil::skipSpace(data, &pos);

      while (pos < len && data[pos] == ',') {
        ++pos;

        CStrUtil::skipSpace(data, &pos);

        pos1 = pos;

        if (pos < len && (isalpha(data[pos]) || data[pos] == '_')) {
          ++pos;

          while (pos < len && (isalnum(data[pos]) || data[pos] == '_'))
            ++pos;
        }

        if (pos == pos1) {
          std::cerr << "Invalid define '" << data << "' - " <<
                       current_file_ << ":" << current_line_ << "\n";
          return;
        }

        std::string variable = data.substr(pos1, pos - pos1);

        variables.push_back(variable);

        CStrUtil::skipSpace(data, &pos);
      }
    }

    if (pos >= len || data[pos] != ')') {
      std::cerr << "Invalid define '" << data << "' - " <<
                   current_file_ << ":" << current_line_ << "\n";
      return;
    }

    ++pos;
  }

  CStrUtil::skipSpace(data, &pos);

  std::string value = CStrUtil::stripSpaces(data.substr(pos));

  if (value == "")
    value = "1";

  add_define(name, variables, value);
}

void
CPrePro::
process_undef_command(const std::string &data)
{
  if (! context_->active || ! context_->processing)
    return;

  int pos = 0;

  CStrUtil::skipSpace(data, &pos);

  int pos1 = pos;

  CStrUtil::skipNonSpace(data, &pos);

  std::string name = data.substr(pos1, pos - pos1);

  remove_define(name);
}

void
CPrePro::
process_include_command(const std::string &data)
{
  if (! context_->active || ! context_->processing)
    return;

  std::string data1 = replace_defines(data, true);

  int len = data1.size();

  if (len < 2) {
    std::cerr << "Illegal include syntax - " <<
                 current_file_ << ":" << current_line_ << "\n";
    return;
  }

  char c;

  if      (data1[0] == '\"')
    c = '\"';
  else if (data1[0] == '<')
    c = '>';
  else {
    std::cerr << "Illegal include syntax - " <<
                 current_file_ << ":" << current_line_ << "\n";
    return;
  }

  int i = 1;

  for (i = 1; i < len && data1[i] != c; ++i)
    ;

  std::string fileName = data1.substr(1, i - 1);

  bool std = false;

  const std::string include_file = get_include_file(fileName, std);

  if (include_file == "") {
    std::cerr << "Failed to find include file '" << fileName << "' - " <<
                 current_file_ << ":" << current_line_ << "\n";
    return;
  }

  if (std && skip_std_)
    return;

  Include *include = new Include(include_file);

  if (! current_include_)
    current_include_ = new Include("");

  current_include_->includes.push_back(include);

  std::swap(current_include_, include);

  process_file(current_include_->filename);

  std::swap(current_include_, include);
}

void
CPrePro::
process_error_command(const std::string &data)
{
  if (! context_->active || ! context_->processing)
    return;

  std::cerr << data << " - " << current_file_ << ":" << current_line_ << "\n";
}

void
CPrePro::
process_warning_command(const std::string &data)
{
  if (! context_->active || ! context_->processing)
    return;

  std::cerr << data << " - " << current_file_ << ":" << current_line_ << "\n";
}

int
CPrePro::
process_expression(const std::string &expression)
{
  std::string expression1 = replace_defines(expression, true);

  CExprValuePtr value;

  if (! expr_->evaluateExpression(expression1, value))
    return false;

  long integer;

  if (! value.isValid() || ! value->getIntegerValue(integer))
    return false;

  return (integer != 0);
}

void
CPrePro::
output_line(const std::string &line)
{
  if (! context_->active || ! context_->processing)
    return;

  if (quiet_)
    return;

  std::string line1 = remove_comments(line, false);

  std::string line2 = replace_defines(line1, false);

  if (no_blank_lines_) {
    int len = line2.size();
    int pos = 0;

    CStrUtil::skipSpace(line2, &pos);

    if (pos >= len) return;
  }

  std::cout << line2 << "\n";
}

std::string
CPrePro::
replace_trigraphs(const std::string &line)
{
  static const char trigraph_chars1[] = "=/\'()!<>-";
  static const char trigraph_chars2[] = "#\\^[]|{}~";

  std::string line1;

  int len = line.size();

  int j = 0;

  for (int i = 0; i < int(len) - 2; ++i) {
    if (line[i] != '?' || line[i + 1] != '?') continue;

    const char *p = strchr(trigraph_chars1, line[i + 2]);
    if (! p) continue;

    line1 += line.substr(j, i - j);
    line1 += trigraph_chars2[p - trigraph_chars1];

    i += 3;

    j = i;
  }

  line1 += line.substr(j);

  return line1;
}

std::string
CPrePro::
remove_comments(const std::string &line, bool preprocessor_line)
{
  bool in_comment1;

  if (preprocessor_line)
    in_comment1 = false;
  else
    in_comment1 = in_comment_;

  std::string line1;

  int pos = 0;
  int len = line.size();

  while (pos < len) {
    if      (! in_comment1 && line[pos] == '/' && line[pos + 1] == '*') {
      in_comment1 = true;

      pos += 2;
    }
    else if (  in_comment1 && line[pos] == '*' && line[pos + 1] == '/') {
      in_comment1 = false;

      pos += 2;
    }
#ifdef CPP_SUPPORT
    else if (! in_comment1 && line[pos] == '/' && line[pos + 1] == '/') {
      break;
    }
#endif
    else {
      if (! in_comment1)
        line1 += line[pos];

      ++pos;
    }
  }

  if (preprocessor_line)
    in_comment_ = false;
  else
    in_comment_ = in_comment1;

  return line1;
}

std::string
CPrePro::
replace_defines(const std::string &tline, bool preprocessor_line)
{
  if (tline.empty()) return tline;

  //------

  struct ReplaceDefineData data;

  return replace_defines(tline, preprocessor_line, data);
}

std::string
CPrePro::
replace_defines(const std::string &tline, bool preprocessor_line, ReplaceDefineData &data)
{
  std::string line = tline;

  if (data.in_replace_defines > 0) {
    DefineList used_defines2;

    for (const auto &pd : used_defines2)
      used_defines2.push_back(pd);

    data.used_defines_list.push_back(used_defines2);
  }
  else {
    data.used_defines.clear();

    data.used_defines_list.clear();
  }

  //------

  DefineList used_defines1;

  int num_replaced = 0;

  ++data.in_replace_defines;

  while (data.in_replace_defines > int(data.lines1.size()))
    data.lines1.push_back(new std::string);

  int iline1 = data.in_replace_defines - 1;

  *data.lines1[iline1] = "";

  ArgList args;

  int pos = 0;
  int len = line.size();

  while (pos < len) {
    int pos1 = pos;

    bool in_string1 = false;
    bool in_string2 = false;

    while (pos < len) {
      if (! in_string1 && ! in_string2) {
        if      (isalpha(line[pos]) || line[pos] == '_')
          break;
        else if (line[pos] == '\'')
          in_string1 = ! in_string1;
        else if (line[pos] == '\"')
          in_string2 = ! in_string2;
      }
      else if (in_string1) {
        if      (line[pos] == '\\') {
          if (pos < len - 1)
            ++pos;
        }
        else if (line[pos] == '\'')
          in_string1 = ! in_string1;
      }
      else if (in_string2) {
        if      (line[pos] == '\\') {
          if (pos < len - 1)
            ++pos;
        }
        else if (line[pos] == '\"')
          in_string2 = ! in_string2;
      }

      ++pos;
    }

    if (pos1 != pos)
      *data.lines1[iline1] += line.substr(pos1, pos - pos1);

    if (pos >= len)
      break;

    pos1 = pos;

    if (pos < len)
      ++pos;

    while (pos < len && (isalnum(line[pos]) || line[pos] == '_'))
      ++pos;

    std::string identifier = line.substr(pos1, pos - pos1);

    if (preprocessor_line && identifier == "defined" &&
        pos < len && line[pos] == '(') {
      ++pos;

      pos1 = pos;

      while (pos < len && line[pos] != ')')
        ++pos;

      std::string identifier = line.substr(pos1, pos - pos1);

      if (pos < len && line[pos] == ')')
        ++pos;

      Define *define = get_define(identifier);

      if (define)
        *data.lines1[iline1] += "1";
      else
        *data.lines1[iline1] += "0";

      continue;
    }

    Define *define = get_define(identifier);

    if (! define) {
      *data.lines1[iline1] += identifier;

      continue;
    }

    DefineList::const_iterator pd1, pd2;

    for (pd1 = data.used_defines.begin(), pd2 = data.used_defines.end(); pd1 != pd2; ++pd1)
      if ((*pd1) == define)
        break;

    if (pd1 != pd2) {
      *data.lines1[iline1] += identifier;

      continue;
    }

    if (define->variables.empty()) {
      used_defines1.push_back(define);

      *data.lines1[iline1] += define->value;

      num_replaced++;

      continue;
    }

    pos1 = pos;

    CStrUtil::skipSpace(line, &pos1);

    if (pos1 < len && line[pos1] != '(') {
      *data.lines1[iline1] += identifier;

      continue;
    }

    pos = pos1;

    args.clear();

    int pos2 = pos;

    ++pos;

    while (pos < len) {
      CStrUtil::skipSpace(line, &pos);

      pos1 = pos;

      int  brackets   = 0;
      bool in_string1 = false;
      bool in_string2 = false;

      while (pos < len) {
        if (! in_string1 && ! in_string2) {
          if      (line[pos] == '(')
            ++brackets;
          else if (line[pos] == ')') {
            if (brackets <= 0)
              break;

            --brackets;
          }
          else if (line[pos] == ',') {
            if (brackets <= 0)
              break;
          }
          else if (line[pos] == '\'')
            in_string1 = ! in_string1;
          else if (line[pos] == '\"')
            in_string2 = ! in_string2;
        }
        else if (in_string1) {
          if      (line[pos] == '\\') {
            if (pos < len - 1)
              ++pos;
          }
          else if (line[pos] == '\'')
            in_string1 = ! in_string1;
        }
        else if (in_string2) {
          if      (line[pos] == '\\') {
            if (pos < len - 1)
              ++pos;
          }
          else if (line[pos] == '\"')
            in_string2 = ! in_string2;
        }

        ++pos;
      }

      std::string arg = line.substr(pos1, pos - pos1);

      arg = CStrUtil::stripSpaces(arg);

      args.push_back(arg);

      if (pos >= len || line[pos] == ')') {
        if (pos < len)
          ++pos;

        break;
      }

      if (pos < len && line[pos] == ',')
        ++pos;
    }

    //------

    int num_args = args.size();

    int num_variables = define->variables.size();

    if (num_args != num_variables) {
      args.clear();

      *data.lines1[iline1] += identifier;

      pos = pos2;

      continue;
    }

    bool hash_hash_before = false;
    bool hash_hash_after  = false;

    int len1 = define->value.size();

    pos1 = 0;

    while (pos1 < len1) {
      pos2 = pos1;

      bool hash_before = false;

      hash_hash_before = false;

      int pos3;

      while (pos1 < len1) {
        hash_before      = false;
        hash_hash_before = false;

        if (define->value[pos1] == '#') {
          pos3 = pos1;

          ++pos1;

          if (define->value[pos1] == '#') {
            ++pos1;

            hash_hash_before = true;
          }
          else
            hash_before = true;

          CStrUtil::skipSpace(define->value, &pos1);
        }

        if (pos1 < len1 && (isalpha(define->value[pos1]) || define->value[pos1] == '_'))
          break;

        ++pos1;
      }

      if (hash_before) {
        if (pos3 != pos2)
          *data.lines1[iline1] += define->value.substr(pos2, pos3 - pos2);
      }
      else {
        if (pos1 != pos2)
          *data.lines1[iline1] += define->value.substr(pos2, pos1 - pos2);
      }

      if (pos1 >= len1)
        break;

      pos2 = pos1;

      ++pos1;

      while (pos < len1 && (isalnum(define->value[pos1]) || define->value[pos1] == '_'))
        ++pos1;

      std::string identifier = define->value.substr(pos2, pos1 - pos2);

      int i = 0;

      for (i = 0; i < num_variables; i++)
        if (define->variables[i] == identifier)
          break;

      if (i >= num_variables) {
        *data.lines1[iline1] += define->value.substr(pos2, pos1 - pos2);

        continue;
      }

      pos2 = pos1;

      CStrUtil::skipSpace(define->value, &pos2);

      if (pos2 < len1 - 1 && define->value[pos2] == '#' && define->value[pos2 + 1] == '#')
        hash_hash_after = true;
      else
        hash_hash_after = false;

      if (hash_before) {
        *data.lines1[iline1] += "\"";

        pos2 = 0;
        len1 = args[i].size();

        while (pos2 < len1) {
          if (args[i][pos2] == '"' || args[i][pos2] == '\\')
            *data.lines1[iline1] += "\\";

          *data.lines1[iline1] += args[i][pos2];

          ++pos2;
        }

        *data.lines1[iline1] += "\"";
      }
      else {
        std::string args1;

        if (! hash_hash_before && ! hash_hash_after)
          args1 = replace_defines(args[i], false);
        else
          args1 = args[i];

        *data.lines1[iline1] += args1;
      }

      ++num_replaced;
    }

    used_defines1.push_back(define);

    args.clear();

    *data.lines1[iline1] = replace_hash_hash(*data.lines1[iline1]);
  }

  std::string line2;

  if (num_replaced > 0) {
    for (const auto &pd : used_defines1)
      data.used_defines.push_back(pd);

    line2 = replace_defines(*data.lines1[iline1], false, data);
  }
  else
    line2 = *data.lines1[iline1];

  used_defines1.clear();

  *data.lines1[data.in_replace_defines - 1] = *data.lines1[iline1];

  --data.in_replace_defines;

  data.used_defines.clear();

  if (data.in_replace_defines > 0)
    data.used_defines_list.pop_back();

  return line2;
}

std::string
CPrePro::
replace_hash_hash(const std::string &tline)
{
  std::string line = tline;

  std::string line1;

  int len = line.size();

  int i = 0;

  while (i < len) {
    int i1 = i;

    CStrUtil::skipSpace(line, &i1);

    if (i1 < len - 1 && line[i1] == '#' && line[i1 + 1] == '#') {
      i1 += 2;

      CStrUtil::skipSpace(line, &i1);

      i = i1;

      if (i >= len)
        break;
    }

    line1 += line[i++];
  }

  return line1;
}

void
CPrePro::
add_file(const std::string &fileName)
{
  files_.push_back(fileName);
}

void
CPrePro::
add_define(const std::string &name, const VariableList &variables, const std::string &value)
{
  if (debug_) {
    if (! variables.empty()) {
      std::cerr << "Add Define " << name << "(";

      int num_variables = variables.size();

      for (int i = 0; i < num_variables; i++) {
        if (i > 0)
          std::cerr << ", ";

        std::cerr << variables[i];
      }

      std::cerr << ")=" << value << "\n";
    }
    else
      std::cerr << "Add Define " << name << "=" << value << "\n";
  }

  Define *define = get_define(name);

  if (! define) {
    define = new Define(name, variables, value);

    defines_.push_back(define);

    return;
  }

  bool redefined = false;

  if (define->value != value)
    redefined = true;

  if (define->variables.size() != variables.size())
    redefined = true;

  if (! redefined && ! define->variables.empty()) {
    int num_variables = define->variables.size();

    for (int i = 0; i < num_variables; i++) {
      if (define->variables[i] != variables[i]) {
        redefined = true;
        break;
      }
    }
  }

  if (redefined)
    std::cerr << "Redefinition of " << name << " from " << define->value << " to " << value <<
                 " - " << current_file_ << ":" << current_line_ << "\n";

  define->variables = variables;
  define->value     = value;
}

void
CPrePro::
remove_define(const std::string &name)
{
  Define *define = get_define(name);

  if (! define)
    return;

  defines_.remove(define);

  delete define;
}

CPrePro::Define *
CPrePro::
get_define(const std::string &name)
{
  for (const auto &pd : defines_)
    if (pd->name == name)
      return pd;

  return nullptr;
}

void
CPrePro::
add_include_dir(const std::string &dirName, bool std)
{
  if (std)
    std_include_dirs_.push_back(dirName);
  else
    include_dirs_.push_back(dirName);
}

std::string
CPrePro::
get_include_file(const std::string &fileName, bool &std)
{
  std = false;

  if (CFile::exists(fileName))
    return fileName;

  for (const auto &dir : include_dirs_) {
    std::string fileName1 = dir + "/" + fileName;

    if (CFile::exists(fileName1))
      return fileName1;
  }

  std = true;

  for (const auto &dir : std_include_dirs_) {
    std::string fileName1 = dir + "/" + fileName;

    if (CFile::exists(fileName1))
      return fileName1;
  }

  std::string fileName1 = "/usr/include/" + fileName;

  if (CFile::exists(fileName1))
    return fileName1;

  return "";
}

void
CPrePro::
start_context(bool processing)
{
  Context *old_context = context_;

  if (old_context)
    context_stack_.push_back(old_context);

  context_ = new Context;

  if (old_context)
    context_->active = old_context->processing;
  else
    context_->active = true;

  context_->processing = processing;
  context_->processed  = processing;
}

bool
CPrePro::
end_context()
{
  bool flag = true;

  delete context_;

  if (! context_stack_.empty()) {
    context_ = context_stack_.back();

    context_stack_.pop_back();
  }
  else
    flag = false;

  if (! context_) {
    context_ = new Context;

    context_->active     = true;
    context_->processed  = false;
    context_->processing = true;
  }

  return flag;
}

CExprValuePtr
CPrePro::
defined_proc(const CExprValuePtr *values, int)
{
  CExprValuePtr value;

  long integer;

  if (values[0].isValid() && values[0]->getIntegerValue(integer))
    value = expr_->createIntegerValue(integer);
  else
    value = expr_->createIntegerValue(0);

  return value;
}

void
CPrePro::
terminate()
{
  if (list_includes_) {
    if (current_include_)
      current_include_->print();
  }
}
