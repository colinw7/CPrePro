#include <CExpr.h>
#include <CFile.h>
#include <CStrUtil.h>
#include <cstring>

typedef std::vector<std::string> VariableList;

struct Context {
  bool active;
  bool processed;
  bool processing;
};

struct Define {
  std::string  name;
  VariableList variables;
  std::string  value;
};

typedef std::vector<Context *>     ContextStack;
typedef std::list<Define *>        DefineList;
typedef std::list<DefineList>      DefineListList;
typedef std::vector<std::string>   FileList;
typedef std::vector<std::string>   DirList;
typedef std::vector<std::string>   ArgList;
typedef std::vector<std::string *> LineList;

struct ReplaceDefineData {
  ReplaceDefineData() { }

  DefineList     used_defines;
  DefineListList used_defines_list;
  uint           in_replace_defines { 0 };
  LineList       lines1;
  std::string    identifier;
};

static void        initialize
                    ();
static void        process_args
                    (int argc, char **argv);
static uint        process_option
                    (const std::string &option, const std::string &arg);
static void        add_define_option
                    (const std::string &define);
static void        add_include_option
                    (const std::string &define);
static void        process_arg
                    (const std::string &arg);
static void        process_files
                    ();
static void        process_file
                    (const std::string &file);
static void        process_line
                    (const std::string &line);
static void        process_command
                    (const std::string &command, const std::string &data);
static void        process_if_command
                    (const std::string &data);
static void        process_ifdef_command
                    (const std::string &data);
static void        process_ifndef_command
                    (const std::string &data);
static void        process_else_command
                    (const std::string &data);
static void        process_elif_command
                    (const std::string &data);
static void        process_endif_command
                    (const std::string &data);
static void        process_define_command
                    (const std::string &data);
static void        process_undef_command
                    (const std::string &data);
static void        process_include_command
                    (const std::string &data);
static void        process_error_command
                    (const std::string &data);
static int         process_expression
                    (const std::string &expression);
static void        output_line
                    (const std::string &line);
static std::string replace_trigraphs
                    (const std::string &line);
static std::string remove_comments
                    (const std::string &line, bool preprocessor_line);
static std::string replace_defines
                    (const std::string &line, bool preprocessor_line);
static std::string replace_defines
                    (const std::string &tline, bool preprocessor_line, ReplaceDefineData &data);
static std::string replace_hash_hash
                    (const std::string &line);
static void        add_file
                    (const std::string &file);
static void        add_define
                    (const std::string &name, const VariableList &variables,
                     const std::string &value);
static void        remove_define
                    (const std::string &name);
static Define     *get_define
                    (const std::string &name);
static void        add_include_dir
                    (const std::string &dir);
static std::string get_include_file
                    (const std::string &file);
static void        start_context
                    (bool processing);
static bool        end_context
                    ();
static void        terminate
                    ();
static CExprValuePtr defined_proc
                      (const CExprValuePtr *values, uint num_values);
static FileList       files;
static DefineList     defines;
static DirList        include_dirs;
static Context       *context        = NULL;
static ContextStack   context_stack;
static bool           no_blank_lines = false;
static bool           echo_input     = false;
static bool           debug          = false;
static std::string    current_file   = "None";
static uint           current_line   = 0;
static bool           in_comment     = false;
static CExpr         *expr           = nullptr;

static const char trigraph_chars1[] = "=/\'()!<>-";
static const char trigraph_chars2[] = "#\\^[]|{}~";

class DefinedFunction : public CExprFunctionObj {
 public:
  CExprValuePtr operator()(CExpr *, const CExprValueArray &values) {
    return defined_proc(&values[0], values.size());
  }
};

extern int
main(int argc, char **argv)
{
  initialize();

  process_args(argc, argv);

  process_files();

  terminate();

  return 0;
}

static void
initialize()
{
  expr = new CExpr;

  expr->addFunction("defined", "nis", new DefinedFunction);

  context_stack.clear();

  start_context(true);
}

static void
process_args(int argc, char **argv)
{
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (argv[i][1] != '\0') {
        uint num_args_processed = process_option(&argv[i][1], argv[i + 1]);

        i += num_args_processed - 1;
      }
    }
    else
      process_arg(argv[i]);
  }
}

static uint
process_option(const std::string &option, const std::string &)
{
  uint num_args_processed = 1;

  if      (option[0] == 'D')
    add_define_option(option.substr(1));
  else if (option[0] == 'I')
    add_include_option(option.substr(1));
  else if (option == "stdin")
    add_file(NULL);
  else if (option == "no_blank_lines")
    no_blank_lines = true;
  else if (option == "echo")
    echo_input = true;
  else if (option == "debug")
    debug = true;
  else
    std::cerr << "Invalid Option " << option << std::endl;

  return num_args_processed;
}

static void
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

static void
add_include_option(const std::string &str)
{
  add_include_dir(str);
}

static void
process_arg(const std::string &arg)
{
  add_file(arg);
}

static void
process_files()
{
  uint num_files = files.size();

  if (num_files == 0)
    add_file("");

  for (uint i = 0; i < num_files; i++)
    process_file(files[i]);
}

static void
process_file(const std::string &fileName)
{
  std::string save_current_file = current_file;
  uint        save_current_line = current_line;

  if (fileName != "")
    current_file = fileName;
  else
    current_file = "<stdin>";

  current_line = 0;

  if (debug)
    std::cerr << "Processing file " << current_file << std::endl;

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

  uint num_lines = lines.size();

  for (uint i = 0; i < num_lines; ++i) {
    std::string line1 = replace_trigraphs(lines[i]);

    uint len = line1.size();

    if (len > 0 && line1[len - 1] == '\\') {
      if (echo_input)
        std::cerr << line1 << std::endl;

      std::string line2 = line1.substr(0, len - 1);

      ++i;

      line1 = replace_trigraphs(lines[i]);

      len = line1.size();

      while (i < num_lines && len > 0 && line1[len - 1] == '\\') {
        if (echo_input)
          std::cerr << line1 << std::endl;

        line2 += line1.substr(0, len - 1);

        ++i;

        line1 = replace_trigraphs(lines[i]);

        len = line1.size();
      }

      if (i < num_lines) {
        if (echo_input)
          std::cerr << line1 << std::endl;

        line2 += line1;
      }
      else
        i--;

      current_line = i + 1;

      if (! in_comment && line2[0] == '#')
        process_line(line2);
      else
        output_line(line2);
    }
    else {
      current_line = i + 1;

      if (echo_input)
        std::cerr << line1 << std::endl;

      if (! in_comment && line1[0] == '#')
        process_line(line1);
      else
        output_line(line1);
    }
  }

  current_file = save_current_file;
  current_line = save_current_line;
}

static void
process_line(const std::string &line)
{
  static std::string command;

  std::string line1 = remove_comments(line, true);

  uint pos = 1;

  CStrUtil::skipSpace(line1, &pos);

  uint pos1 = pos;

  CStrUtil::skipNonSpace(line1, &pos);

  command = line1.substr(pos1, pos - pos1);

  CStrUtil::skipSpace(line1, &pos);

  if (command != "")
    process_command(command, line1.substr(pos));
}

static void
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
  else if (command == "pragma" )
    ;
  else
    std::cerr << "Command '" << command << "' not supported - " <<
            current_file << ":" << current_line << std::endl;
}

static void
process_if_command(const std::string &data)
{
  start_context(false);

  if (context->active)
    context->processing = process_expression(data);
  else
    context->processing = false;

  context->processed = context->processing;
}

static void
process_ifdef_command(const std::string &data)
{
  start_context(false);

  if (context->active) {
    Define *define = get_define(data);

    context->processing = (define != NULL);
  }
  else
    context->processing = false;

  context->processed = context->processing;
}

static void
process_ifndef_command(const std::string &data)
{
  start_context(false);

  if (context->active) {
    Define *define = get_define(data);

    context->processing = (define == NULL);
  }
  else
    context->processing = false;

  context->processed = context->processing;
}

static void
process_else_command(const std::string &)
{
  if (context->active) {
    context->processing = ! context->processed;

    context->processed = true;
  }
  else
    context->processing = false;
}

static void
process_elif_command(const std::string &data)
{
  if (context->active) {
    bool flag = process_expression(data);

    if (flag)
      context->processing = ! context->processed;

    if (context->processing)
      context->processed = true;
  }
  else
    context->processing = false;
}

static void
process_endif_command(const std::string &)
{
  if (! end_context())
    std::cerr << "if/endif mismatch - " << current_file << ":" << current_line << std::endl;
}

static void
process_define_command(const std::string &data)
{
  if (! context->active || ! context->processing)
    return;

  uint pos = 0;
  uint len = data.size();

  CStrUtil::skipSpace(data, &pos);

  uint pos1 = pos;

  if (pos < len && (isalpha(data[pos]) || data[pos] == '_')) {
    ++pos;

    while (pos < len && (isalnum(data[pos]) || data[pos] == '_'))
      ++pos;
  }

  if (pos == pos1) {
    std::cerr << "Invalid define '" << data << "' - " <<
                 current_file << ":" << current_line << std::endl;
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
                     current_file << ":" << current_line << std::endl;
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
                       current_file << ":" << current_line << std::endl;
          return;
        }

        std::string variable = data.substr(pos1, pos - pos1);

        variables.push_back(variable);

        CStrUtil::skipSpace(data, &pos);
      }
    }

    if (pos >= len || data[pos] != ')') {
      std::cerr << "Invalid define '" << data << "' - " <<
                   current_file << ":" << current_line << std::endl;
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

static void
process_undef_command(const std::string &data)
{
  if (! context->active || ! context->processing)
    return;

  uint pos = 0;

  CStrUtil::skipSpace(data, &pos);

  uint pos1 = pos;

  CStrUtil::skipNonSpace(data, &pos);

  std::string name = data.substr(pos1, pos - pos1);

  remove_define(name);
}

static void
process_include_command(const std::string &data)
{
  if (! context->active || ! context->processing)
    return;

  uint len = data.size();

  if (len < 2) {
    std::cerr << "Illegal include syntax - " <<
                 current_file << ":" << current_line << std::endl;
    return;
  }

  char c;

  if      (data[0] == '\"')
    c = '\"';
  else if (data[0] == '<')
    c = '>';
  else {
    std::cerr << "Illegal include syntax - " << current_file << ":" << current_line << std::endl;
    return;
  }

  uint i = 1;

  for (i = 1; i < len && data[i] != c; ++i)
    ;

  std::string fileName = data.substr(1, i - 1);

  const std::string include_file = get_include_file(fileName);

  if (include_file != "")
    process_file(include_file);
  else
    std::cerr << "Failed to find include file '" << fileName << "' -" <<
                 current_file << ":" << current_line << std::endl;
}

static void
process_error_command(const std::string &data)
{
  if (! context->active || ! context->processing)
    return;

  std::cerr << data << " - " << current_file << ":" << current_line << std::endl;
}

static int
process_expression(const std::string &expression)
{
  std::string expression1 = replace_defines(expression, true);

  CExprValuePtr value;

  if (! expr->evaluateExpression(expression1, value))
    return false;

  long integer;

  if (! value->getIntegerValue(integer))
    return false;

  return (integer != 0);
}

static void
output_line(const std::string &line)
{
  if (! context->active || ! context->processing)
    return;

  std::string line1 = remove_comments(line, false);

  std::string line2 = replace_defines(line1, false);

  if (no_blank_lines) {
    uint len = line2.size();
    uint pos = 0;

    CStrUtil::skipSpace(line2, &pos);

    if (pos >= len) return;
  }

  std::cout << line2 << std::endl;
}

static std::string
replace_trigraphs(const std::string &line)
{
  std::string line1;

  uint len = line.size();

  uint j = 0;

  for (int i = 0; i < int(len) - 2; ++i) {
    if (line[i] != '?' || line[i + 1] != '?') continue;

    const char *p = strchr(trigraph_chars1, line[i + 2]);

    if (p == NULL) continue;

    line1 += line.substr(j, i - j);
    line1 += trigraph_chars2[p - trigraph_chars1];

    i += 3;

    j = i;
  }

  line1 += line.substr(j);

  return line1;
}

static std::string
remove_comments(const std::string &line, bool preprocessor_line)
{
  bool in_comment1;

  if (preprocessor_line)
    in_comment1 = false;
  else
    in_comment1 = in_comment;

  std::string line1;

  uint pos = 0;
  uint len = line.size();

  while (pos < len) {
    if      (! in_comment1 && line[pos] == '/' && line[pos + 1] == '*') {
      in_comment1 = true;

      pos += 2;
    }
    else if (  in_comment1 && line[pos] == '*' && line[pos + 1] == '/') {
      in_comment1 = false;

      pos += 2;
    }
    else {
      if (! in_comment1)
        line1 += line[pos];

      ++pos;
    }
  }

  if (preprocessor_line)
    in_comment = false;
  else
    in_comment = in_comment1;

  return line1;
}

static std::string
replace_defines(const std::string &tline, bool preprocessor_line)
{
  if (tline.empty()) return tline;

  //------

  struct ReplaceDefineData data;

  return replace_defines(tline, preprocessor_line, data);
}

static std::string
replace_defines(const std::string &tline, bool preprocessor_line, ReplaceDefineData &data)
{
  std::string line = tline;

  if (data.in_replace_defines > 0) {
    DefineList used_defines2;

    DefineList::const_iterator pd1, pd2;

    for (pd1 = used_defines2.begin(), pd2 = used_defines2.end(); pd1 != pd2; ++pd1)
      used_defines2.push_back(*pd1);

    data.used_defines_list.push_back(used_defines2);
  }
  else {
    data.used_defines.clear();

    data.used_defines_list.clear();
  }

  //------

  DefineList used_defines1;

  uint num_replaced = 0;

  ++data.in_replace_defines;

  while (data.in_replace_defines > data.lines1.size())
    data.lines1.push_back(new std::string);

  uint iline1 = data.in_replace_defines - 1;

  *data.lines1[iline1] = "";

  ArgList args;

  uint pos = 0;
  uint len = line.size();

  while (pos < len) {
    uint pos1 = pos;

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

      if (define != NULL)
        *data.lines1[iline1] += "1";
      else
        *data.lines1[iline1] += "0";

      continue;
    }

    Define *define = get_define(identifier);

    if (define == NULL) {
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

    uint pos2 = pos;

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

    uint num_args = args.size();

    uint num_variables = define->variables.size();

    if (num_args != num_variables) {
      args.clear();

      *data.lines1[iline1] += identifier;

      pos = pos2;

      continue;
    }

    bool hash_hash_before = false;
    bool hash_hash_after  = false;

    uint len1 = define->value.size();

    pos1 = 0;

    while (pos1 < len1) {
      pos2 = pos1;

      bool hash_before = false;

      hash_hash_before = false;

      uint pos3;

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

      uint i = 0;

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
    DefineList::const_iterator pd1, pd2;

    for (pd1 = used_defines1.begin(), pd2 = used_defines1.end(); pd1 != pd2; ++pd1)
      data.used_defines.push_back(*pd1);

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

static std::string
replace_hash_hash(const std::string &tline)
{
  std::string line = tline;

  std::string line1;

  uint len = line.size();

  uint i = 0;

  while (i < len) {
    uint i1 = i;

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

static void
add_file(const std::string &fileName)
{
  files.push_back(fileName);
}

static void
add_define(const std::string &name, const VariableList &variables, const std::string &value)
{
  if (debug) {
    if (! variables.empty()) {
      std::cerr << "Add Define " << name << "(";

      int num_variables = variables.size();

      for (int i = 0; i < num_variables; i++) {
        if (i > 0)
          std::cerr << ", ";

        std::cerr << variables[i];
      }

      std::cerr << ")=" << value << std::endl;
    }
    else
      std::cerr << "Add Define " << name << "=" << value << std::endl;
  }

  Define *define = get_define(name);

  if (define == NULL) {
    define = new Define;

    define->name      = name;
    define->variables = variables;
    define->value     = value;

    defines.push_back(define);

    return;
  }

  bool redefined = false;

  if (define->value != value)
    redefined = true;

  if (define->variables.size() != variables.size())
    redefined = true;

  if (! redefined && ! define->variables.empty()) {
    uint num_variables = define->variables.size();

    for (uint i = 0; i < num_variables; i++) {
      if (define->variables[i] != variables[i]) {
        redefined = true;
        break;
      }
    }
  }

  if (redefined)
    std::cerr << "Redefinition of " << name << " from " << define->value <<
                 " to " << value << " - " << current_file << ":" <<
                 current_line << std::endl;

  define->variables = variables;
  define->value     = value;
}

static void
remove_define(const std::string &name)
{
  Define *define = get_define(name);

  if (define == NULL)
    return;

  defines.remove(define);

  delete define;
}

static Define *
get_define(const std::string &name)
{
  DefineList::const_iterator pd1, pd2;

  for (pd1 = defines.begin(), pd2 = defines.end(); pd1 != pd2; ++pd1)
    if ((*pd1)->name == name)
      return *pd1;

  return NULL;
}

static void
add_include_dir(const std::string &dirName)
{
  include_dirs.push_back(dirName);
}

static std::string
get_include_file(const std::string &fileName)
{
  if (CFile::exists(fileName))
    return fileName;

  uint num_include_dirs = include_dirs.size();

  for (uint i = 0; i < num_include_dirs; ++i) {
    std::string fileName1 = include_dirs[i] + "/" + fileName;

    if (CFile::exists(fileName1))
      return fileName1;
  }

  std::string fileName1 = "/usr/include/" + fileName;

  if (CFile::exists(fileName1))
    return fileName1;

  return "";
}

static void
start_context(bool processing)
{
  Context *old_context = context;

  if (old_context != NULL)
    context_stack.push_back(old_context);

  context = new Context;

  if (old_context != NULL)
    context->active = old_context->processing;
  else
    context->active = true;

  context->processing = processing;
  context->processed  = processing;
}

static bool
end_context()
{
  bool flag = true;

  delete context;

  if (! context_stack.empty()) {
    context = context_stack.back();

    context_stack.pop_back();
  }
  else
    flag = false;

  if (context == NULL) {
    context = new Context;

    context->active     = true;
    context->processed  = false;
    context->processing = true;
  }

  return flag;
}

static CExprValuePtr
defined_proc(const CExprValuePtr *values, uint)
{
  CExprValuePtr value;

  long integer;

  if (values[0].isValid() && values[0]->getIntegerValue(integer))
    value = expr->createIntegerValue(integer);
  else
    value = expr->createIntegerValue(0);

  return value;
}

static void
terminate()
{
  exit(0);
}
