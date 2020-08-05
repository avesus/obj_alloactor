#ifndef _ARG_H_
#define _ARG_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <string>
#include <vector>

#include <misc/macro_helper.h>

//////////////////////////////////////////////////////////////////////
// Simple argument parser.

//////////////////////////////////////////////////////////////////////
// API

// Call before any ADD_ARG calls
#define PREPARE_PARSER                                                         \
    {                                                                          \
        args::argument_parser _args_parser;

// Call to complete parsing (must be included after PREPARE_PARSER)
#define PARSE_ARGUMENTS                                                        \
    _args_parser.parse_arguments(argc, argv);                                  \
    }


// Add a new argument
//"short name", "long name" (optional), required, type, variable, "help_message"
#define ADD_ARG(...)                                                           \
    CAT(ADD_ARG_, PP_NARG(__VA_ARGS__))(__VA_ARGS__)

//////////////////////////////////////////////////////////////////////

namespace args {

enum types { Int = 0, String = 1, Float = 2, Set = 3, Help = 4 };
enum exit_codes {
    SUCCESS_PARSE            = 0,
    SUCCESS_HELP             = 1,
    ERROR_PARSING_CMDLINE    = 2,
    ERROR_BAD_VARIABLE       = 3,
    ERROR_BAD_ARGUMENT       = 4,
    ERROR_UNKOWN_TYPE        = 5,
    ERROR_DUPLICATE_ARGUMENT = 6,
    ERROR_DUPLICATE_VARIABLE = 7,
    ERROR_UNKOWN             = 8
};


// class to store information on a given argument (i.e info on "-v",
// "--verbose")
class argument_info {

    // short name of argument
    const char * short_argument;

    // optional longer name (again "-v" vs "--verbose")
    const char * long_argument;

    // help message assosiated with argument
    const char * help_message;

    // name of variable (I personally like this)
    const char * var_name;

    // ptr to variable to set
    void * var;

    // size of said value
    uint32_t var_size;

    // type of variable (only string, int, float, or set)
    types type;

    // wether argument is required
    bool required;

    // whether the argument was found
    bool argument_found;

   public:
    // generall constructor
    argument_info(const char * _short_argument,
                  const char * _long_argument,
                  bool         _required,
                  types        _type,
                  void *       _var,
                  uint32_t     _var_size,
                  const char * _var_name,
                  const char * _help_message);

    // print default value of var
    void print_default() const;

    // prints info (for usage/help/errors)
    void print_arg_info() const;

    // getters
    const char * get_short_argument() const;
    const char * get_long_argument() const;
    types        get_type() const;
    void *       get_var() const;
    uint32_t     get_size() const;
    bool         get_required() const;
    bool         get_found() const;

    // setter
    void set_found();
};

// class responbility for ensuring arguments meet format. I.e ensures type is
// proper, var is proper, and most importantly that the argument name meets
// certain format specifications
class argument_checker {

    // check that var/type/argument meet requirements
    exit_codes check_var(void * var, uint32_t var_size) const;
    exit_codes check_type(types type) const;
    exit_codes check_argument(const char * argument) const;

    // helper for checking argument to ensure leading "-*" is proper
    exit_codes drop_leading_dashes(const char * argument) const;

    // basic test if char is letter/number
    uint32_t not_letter(char c) const;
    uint32_t not_number(char c) const;

   public:
    // tests that char is one of a set of valid options (letter/number/'-')
    uint32_t invalid_argument_char(char c) const;

    // runs all checks
    exit_codes preprocess_argument(const char * short_argument,
                                   const char * long_argument,
                                   types        type,
                                   void *       var,
                                   uint32_t     var_size) const;
};


class argument_parser {

    uint32_t         nreq_remaining;
    argument_checker arg_checker;

    std::vector<argument_info> arguments;

    // prints form for usage (i.e argument, type, etc...)
    void argument_format_print() const;
    void usage() const;

    // exit by printing usage (success) called as result of help
    void exit_usage_success() const;

    // various exit function in case of a variety of errors
    void exit_missing_error() const;
    void exit_usage_error(exit_codes error, const char * fmt, ...) const;
    void exit_error(exit_codes error, argument_info bad_argument) const;

    // finds an argument_info struct that matches a cmdline argument, returns
    // NULL if none found
    argument_info * find_ai_ptr(const char * cmdline_argument);

    // verify that new argument is unique
    exit_codes verify_unique(const char * short_argument,
                             const char * long_argument,
                             void *       var) const;


   public:
    // constructor
    argument_parser();

    // parse cmdline
    void parse_arguments(uint32_t argc, char ** cmdline);

    // add new argument
    void add_argument(const char * short_argument,
                      const char * long_argument,
                      bool         required,
                      types        type,
                      void *       var,
                      uint32_t     var_size,
                      const char * var_name,
                      const char * help_message);
};

}  // namespace args


#define ADD_ARG_5(short_argument, required, type, variable, help_message)      \
    _args_parser.add_argument(short_argument,                                  \
                              NULL,                                            \
                              required,                                        \
                              args::type,                                      \
                              (void *)(&variable),                             \
                              sizeof(decltype(variable)),                      \
                              #variable,                                       \
                              help_message)

#define ADD_ARG_6(short_argument,                                              \
                  long_argument,                                               \
                  required,                                                    \
                  type,                                                        \
                  variable,                                                    \
                  help_message)                                                \
    _args_parser.add_argument(short_argument,                                  \
                              long_argument,                                   \
                              required,                                        \
                              args::type,                                      \
                              (void *)(&variable),                             \
                              sizeof(decltype(variable)),                      \
                              #variable,                                       \
                              help_message)


#endif
