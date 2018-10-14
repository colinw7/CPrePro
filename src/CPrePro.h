#ifndef CPrePro_H
#define CPrePro_H

#include <CExpr.h>
#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <fstream>

class CPrePro {
 public:
  typedef std::vector<std::string> VariableList;

  struct Context {
    bool active     { false };
    bool processed  { false };
    bool processing { false };
  };

  struct Define {
    std::string  name;
    VariableList variables;
    std::string  value;

    Define(const std::string &name, const VariableList &variables, const std::string &value) :
     name(name), variables(variables), value(value) {
    }
  };

  struct Include;

  using Includes = std::vector<Include *>;

  struct Include {
    Include(const std::string &filename) :
     filename(filename) {
    }

    void print(std::ostream &os, int depth=0) {
      for (int i = 0; i < depth; ++i)
        os << " ";

      os << filename << "\n";

      for (const auto &i : includes)
        i->print(os, depth + 1);
    }

    std::string filename;
    Includes    includes;
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
    int            in_replace_defines { 0 };
    LineList       lines1;
    std::string    identifier;
  };

 public:
  CPrePro();
 ~CPrePro();

  void initialize();
  void terminate();

  void process_args(int argc, char **argv);
  void process_option(const std::string &option, int &argc, char **argv);

  void add_define_option(const std::string &define);
  void add_include_option(const std::string &define);

  void process_arg(const std::string &arg);
  void process_files();
  void process_file(const std::string &file);
  void process_line(const std::string &line);
  void process_command(const std::string &command, const std::string &data);
  void process_if_command(const std::string &data);
  void process_ifdef_command(const std::string &data);
  void process_ifndef_command(const std::string &data);
  void process_else_command(const std::string &data);
  void process_elif_command(const std::string &data);
  void process_endif_command(const std::string &data);
  void process_define_command(const std::string &data);
  void process_undef_command(const std::string &data);
  void process_include_command(const std::string &data);
  void process_error_command(const std::string &data);
  void process_warning_command(const std::string &data);
  int  process_expression(const std::string &expression);

  void output_line(const std::string &line);

  std::string replace_trigraphs(const std::string &line);
  std::string remove_comments(const std::string &line, bool preprocessor_line);
  std::string replace_defines(const std::string &line, bool preprocessor_line);
  std::string replace_defines(const std::string &tline, bool preprocessor_line,
                              ReplaceDefineData &data);
  std::string replace_hash_hash(const std::string &line);

  void add_file(const std::string &file);

  void add_define(const std::string &name, const VariableList &variables, const std::string &value);
  void remove_define(const std::string &name);
  Define     *get_define(const std::string &name);

  void add_include_dir(const std::string &dir, bool std=false);
  std::string get_include_file(const std::string &file, bool &std);

  void start_context(bool processing);
  bool end_context();

  CExprValuePtr defined_proc(const CExprValuePtr *values, int num_values);

 private:
  FileList      files_;
  DefineList    defines_;
  DirList       include_dirs_;
  DirList       std_include_dirs_;
  Context*      context_         { nullptr };
  ContextStack  context_stack_;
  bool          no_blank_lines_  { false };
  bool          echo_input_      { false };
  bool          no_std_          { false };
  bool          quiet_           { false };
  bool          warn_            { true };
  bool          debug_           { false };
  bool          list_includes_   { false };
  std::string   current_file_    { "None" };
  int           current_line_    { 0 };
  bool          in_comment_      { false };
  CExpr*        expr_            { nullptr };
  Includes      includes_;
  Include*      current_include_ { nullptr };
  std::string   output_file_;
  std::ofstream output_fstream_;
  std::ostream* output_stream_   { nullptr };
};

#endif
