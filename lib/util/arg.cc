#include <util/arg.h>


namespace args {

static const char * usage_fmt = "%16.16s,%8.8s,%16.16s,%10.10s,\t%s";


static const char types_to_string[5][8] = { "int",
                                            "string",
                                            "float",
                                            "set",
                                            "help" };


static const char exit_codes_to_string[9][32] = { "SUCCESS_PARSE",
                                                  "SUCCESS_HELP",
                                                  "ERROR_PARSING_CMDLINE",
                                                  "ERROR_BAD_VARIABLE",
                                                  "ERROR_BAD_ARGUMENT",
                                                  "ERROR_UNKOWN_TYPE",
                                                  "ERROR_DUPLICATE_ARGUMENT",
                                                  "ERROR_DUPLICATE_VARIABLE",
                                                  "ERROR_UNKOWN" };


argument_info::argument_info(const char * _short_argument,
                             const char * _long_argument,
                             bool         _required,
                             types        _type,
                             void *       _var,
                             uint32_t     _var_size,
                             const char * _var_name,
                             const char * _help_message) {

    short_argument = _short_argument;
    long_argument  = _long_argument;
    argument_found = false;
    required       = _required;
    type           = _type;
    var            = _var;
    var_size       = _var_size;
    var_name       = _var_name;
    help_message   = _help_message;
}

const char *
argument_info::get_short_argument() const {
    return this->short_argument;
}

const char *
argument_info::get_long_argument() const {
    return this->long_argument;
}
types
argument_info::get_type() const {
    return this->type;
}

void *
argument_info::get_var() const {
    return this->var;
}

uint32_t
argument_info::get_size() const {
    return this->var_size;
}

bool
argument_info::get_required() const {
    return this->required;
}

void
argument_info::set_found() {
    this->argument_found = true;
}

bool
argument_info::get_found() const {
    return this->argument_found;
}

void
argument_info::print_default() const {
    fprintf(stderr, " [Default = ");
    switch (this->type) {
        case Int:
            if (this->var_size == sizeof(int8_t)) {
                fprintf(stderr, "%d", *((int8_t *)this->var));
            }
            else if (this->var_size == sizeof(int16_t)) {
                fprintf(stderr, "%d", *((int16_t *)this->var));
            }
            else if (this->var_size == sizeof(int32_t)) {
                fprintf(stderr, "%d", *((int32_t *)this->var));
            }
            else if (this->var_size == sizeof(uint64_t)) {
                fprintf(stderr, "%ld", *((int64_t *)this->var));
            }
            else {
                fprintf(stderr, "N/A");
            }
            break;
        case String:
            if (*((char **)(this->var)) == NULL) {
                fprintf(stderr, "N/A");
            }
            else {
                fprintf(stderr, "%s", *((char **)(this->var)));
            }
            break;
        case Float:
            if (this->var_size == sizeof(float)) {
                fprintf(stderr, "%f", *((float *)this->var));
            }
            else if (this->var_size == sizeof(double)) {
                fprintf(stderr, "%lf", *((double *)this->var));
            }
            else {
                fprintf(stderr, "N/A");
            }
            break;
        case Set:
            if (this->var_size == sizeof(int8_t)) {
                fprintf(stderr, "%d", !!(*((int8_t *)this->var)));
            }
            else if (this->var_size == sizeof(int16_t)) {
                fprintf(stderr, "%d", !!(*((int16_t *)this->var)));
            }
            else if (this->var_size == sizeof(int32_t)) {
                fprintf(stderr, "%d", !!(*((int32_t *)this->var)));
            }
            else if (this->var_size == sizeof(uint64_t)) {
                fprintf(stderr, "%d", !!(*((int64_t *)this->var)));
            }
            else {
                fprintf(stderr, "N/A");
            }
            break;
        case Help:
            fprintf(stderr, "Help");
            break;
        default:
            fprintf(stderr,
                    "Something really weird went wrong that I "
                    "don't understand\n");
            abort();
    }
    fprintf(stderr, "]");
}

void
argument_info::print_arg_info() const {
    // print goes: short_name, long_name type, var_name, required, help
    std::string full_argument_name(this->short_argument);
    if (this->long_argument) {
        full_argument_name += ", ";
        full_argument_name += this->long_argument;
    }
    fprintf(stderr,
            usage_fmt,
            full_argument_name.c_str(),
            types_to_string[this->type],
            this->var_name,
            this->required ? "True" : "False",
            this->help_message);
    if (this->required == false) {
        this->print_default();
    }
    fprintf(stderr, "\n");
}


exit_codes
argument_checker::check_var(void * var, uint32_t var_size) const {
    // null or invalid VM address are two easiest checks
    if (!var || ((uint64_t)var) > (((1UL) << 48) - 1)) {
        return ERROR_BAD_VARIABLE;
    }
    if (var_size != sizeof(int8_t) && var_size != sizeof(int16_t) &&
        var_size != sizeof(int32_t) && var_size != sizeof(int64_t)) {
        return ERROR_BAD_VARIABLE;
    }
    // any type we are parsing must have size as power of 2
    return SUCCESS_PARSE;
}

exit_codes
argument_checker::check_type(types type) const {
    // this is basically to check that user didnt cant int or something
    if (type != Int && type != String && type != Float && type != Set &&
        type != Help) {
        return ERROR_UNKOWN_TYPE;
    }
    return SUCCESS_PARSE;
}
exit_codes
argument_checker::drop_leading_dashes(const char * argument) const {
    uint32_t leading_dashes = 0;

    // drop leading dashes
    while ((argument)[leading_dashes] && (argument)[leading_dashes] == '-') {
        ++leading_dashes;
    }
    // saying no to more than prefix of "--" or if we reached null the
    // argument was purely dashes.
    if (leading_dashes == 0 || leading_dashes >= 3 ||
        !(argument)[leading_dashes]) {
        return ERROR_BAD_ARGUMENT;
    }
    return SUCCESS_PARSE;
}


exit_codes
argument_checker::check_argument(const char * argument) const {
    bool last_was_dash = false;

    while (*argument) {

        if (this->invalid_argument_char(*argument)) {
            return ERROR_BAD_ARGUMENT;
        }
        else if (last_was_dash && (*argument) == '-') {
            return ERROR_BAD_ARGUMENT;
        }
        last_was_dash = (*argument) == '-';
        ++argument;
    }

    return SUCCESS_PARSE;
}

uint32_t
argument_checker::not_letter(char c) const {
    return (c < 65) || (c > 90 && c < 97) || (c > 122);
}
uint32_t
argument_checker::not_number(char c) const {
    return (c < 48) || (c > 57);
}

uint32_t
argument_checker::invalid_argument_char(char c) const {
    return this->not_letter(c) && this->not_number(c) && (c != '-');
}

exit_codes
argument_checker::preprocess_argument(const char * short_argument,
                                      const char * long_argument,
                                      types        type,
                                      void *       var,
                                      uint32_t     var_size) const {
    exit_codes res;

    if (type != Help) {
        res = check_var(var, var_size);
        if (res != SUCCESS_PARSE) {
            return res;
        }
    }

    res = check_type(type);
    if (res != SUCCESS_PARSE) {
        return res;
    }

    if (long_argument) {
        res = this->drop_leading_dashes(long_argument);
        if (res != SUCCESS_PARSE) {
            return res;
        }
    }

    res = this->drop_leading_dashes(short_argument);
    if (res != SUCCESS_PARSE) {
        return res;
    }


    if (long_argument) {
        res = this->check_argument(long_argument + 1);
        if (res != SUCCESS_PARSE) {
            return res;
        }
    }

    return res = this->check_argument(short_argument + 1);
}


void
argument_parser::argument_format_print() const {
    fprintf(stderr,
            usage_fmt,
            "argument",
            "type",
            "variable name",
            "required",
            "help message\n");
}

void
argument_parser::usage() const {
    this->argument_format_print();
    for (auto it = this->arguments.begin(); it != this->arguments.end(); ++it) {
        it->print_arg_info();
    }
}

void
argument_parser::exit_missing_error() const {
    this->usage();
    fprintf(stderr, "\nError missing required argument\n\n");

    for (auto it = this->arguments.begin(); it != this->arguments.end(); ++it) {
        if (it->get_found() == false && it->get_required() == true) {
            it->print_arg_info();
        }
    }
    exit(ERROR_PARSING_CMDLINE);
}
void
argument_parser::exit_usage_success() const {
    this->usage();
    exit(SUCCESS_PARSE);
}
void
argument_parser::exit_usage_error(exit_codes   error,
                                  const char * fmt,
                                  ...) const {
    fprintf(stderr,
            "Error Processing Arguments: %s ->\n\t",
            exit_codes_to_string[error]);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);  // NOLINT /* Bug in Clang-Tidy */
    va_end(ap);
    fprintf(stderr, "\n");

    this->usage();
    exit(error);
}
void
argument_parser::exit_error(exit_codes    error,
                            argument_info bad_argument) const {
    fprintf(stderr,
            "Error Processing Arguments: %s ->\n\n",
            exit_codes_to_string[error]);

    this->argument_format_print();
    bad_argument.print_arg_info();
    exit(error);
}

argument_info *
argument_parser::find_ai_ptr(const char * cmdline_argument) {
    for (auto it = this->arguments.begin(); it != this->arguments.end(); ++it) {
        if (!strcmp(it->get_short_argument(), cmdline_argument)) {
            return &(*it);
        }
        else if (it->get_long_argument() &&
                 (!strcmp(it->get_long_argument(), cmdline_argument))) {
            return &(*it);
        }
    }
    return NULL;
}

exit_codes
argument_parser::verify_unique(const char * short_argument,
                               const char * long_argument,
                               void *       var) const {
    for (auto it = this->arguments.begin(); it != this->arguments.end(); ++it) {
        if (it->get_var() == var) {
            return ERROR_DUPLICATE_VARIABLE;
        }
        else if (!strcmp(it->get_short_argument(), short_argument)) {
            return ERROR_DUPLICATE_ARGUMENT;
        }
        else if (it->get_long_argument() &&
                 !strcmp(it->get_long_argument(), short_argument)) {
            return ERROR_DUPLICATE_ARGUMENT;
        }
        else if (long_argument &&
                 (!strcmp(it->get_short_argument(), long_argument))) {
            return ERROR_DUPLICATE_ARGUMENT;
        }
        else if (long_argument && it->get_long_argument() &&
                 (!strcmp(it->get_long_argument(), long_argument))) {
            return ERROR_DUPLICATE_ARGUMENT;
        }
    }
    return SUCCESS_PARSE;
}

argument_parser::argument_parser() {
    nreq_remaining = 0;
    this->add_argument("-h",
                       "--help",
                       false,
                       Help,
                       NULL,
                       0,
                       "N/A",
                       "Print Usage");
}

void
argument_parser::parse_arguments(uint32_t argc, char ** cmdline) {

    bool            expecting_value = false;
    argument_info * cur_argument;
    for (uint32_t i = 1; i < argc; i++) {
        if (expecting_value) {
            expecting_value = false;
            char * endptr   = NULL;

            uint64_t temp_int_type;

            void * temp_float_ptr = NULL;
            float  temp_float_4_type;
            double temp_float_8_type;

            char ** temp_str_type;

            switch (cur_argument->get_type()) {
                case Int:
                    endptr        = cmdline[i];
                    temp_int_type = strtol(cmdline[i], &endptr, 10);
                    if (endptr == cmdline[i]) {
                        this->exit_usage_error(
                            ERROR_PARSING_CMDLINE,
                            "Int type had not parseable int value\n");
                    }
                    memcpy(cur_argument->get_var(),
                           (void *)(&temp_int_type),
                           cur_argument->get_size());
                    break;
                case String:
                    temp_str_type  = (char **)cur_argument->get_var();
                    *temp_str_type = cmdline[i];
                    break;
                case Float:
                    endptr = cmdline[i];

                    // 4 byte and 8 byte float have different layout so cant
                    // just get as larget version and memcpy like with int
                    if (cur_argument->get_size() == sizeof(float)) {
                        temp_float_4_type = strtof(cmdline[i], &endptr);
                        temp_float_ptr    = (void *)(&temp_float_4_type);
                    }
                    else if (cur_argument->get_size() == sizeof(double)) {
                        temp_float_8_type = strtod(cmdline[i], &endptr);
                        temp_float_ptr    = (void *)(&temp_float_8_type);
                    }
                    else {
                        this->exit_usage_error(ERROR_PARSING_CMDLINE,
                                               "Float type of unknown size\n");
                    }

                    if (endptr == cmdline[i]) {
                        this->exit_usage_error(
                            ERROR_PARSING_CMDLINE,
                            "Float type had not parseable float "
                            "value\n");
                    }
                    memcpy(cur_argument->get_var(),
                           temp_float_ptr,
                           cur_argument->get_size());
                    break;
                default:
                    fprintf(stderr,
                            "Something really weird went wrong that I "
                            "don't understand\n");
                    abort();
            }
        }
        else {

            cur_argument = find_ai_ptr(cmdline[i]);
            if (cur_argument == NULL) {
                this->exit_usage_error(ERROR_PARSING_CMDLINE,
                                       "Error argument \"%s\" unknown\n",
                                       cmdline[i]);
            }
            else if (cur_argument->get_found() == true) {
                this->exit_usage_error(
                    ERROR_PARSING_CMDLINE,
                    "Error argument \"%s\" specified twice\n",
                    cmdline[i]);
            }

            cur_argument->set_found();
            this->nreq_remaining -= (cur_argument->get_required() ? 1 : 0);

            switch (cur_argument->get_type()) {
                case Int:
                    expecting_value = true;
                    break;
                case String:
                    expecting_value = true;
                    break;
                case Float:
                    expecting_value = true;
                    break;
                case Set:
                    // set single byte to 1
                    memset(cur_argument->get_var(), 1, 1);
                    break;
                case Help:
                    this->exit_usage_success();
                    break;
                default:
                    this->exit_usage_error(ERROR_PARSING_CMDLINE,
                                           "Invalid type detected\n");
                    break;
            }
        }
    }
    if (expecting_value) {
        this->exit_usage_error(ERROR_PARSING_CMDLINE,
                               "Error argument for \"%s\" not provided\n");
    }
    if (this->nreq_remaining) {
        this->exit_missing_error();
    }
}

void
argument_parser::add_argument(const char * short_argument,
                              const char * long_argument,
                              bool         required,
                              types        type,
                              void *       var,
                              uint32_t     var_size,
                              const char * var_name,
                              const char * help_message) {


    exit_codes res = this->arg_checker.preprocess_argument(short_argument,
                                                           long_argument,
                                                           type,
                                                           var,
                                                           var_size);

    if (res != SUCCESS_PARSE) {
        this->exit_error(res,
                         argument_info(short_argument,
                                       long_argument,
                                       required,
                                       type,
                                       var,
                                       var_size,
                                       var_name,
                                       help_message));
    }

    res = this->verify_unique(short_argument, long_argument, var);

    if (res != SUCCESS_PARSE) {
        this->exit_error(res,
                         argument_info(short_argument,
                                       long_argument,
                                       required,
                                       type,
                                       var,
                                       var_size,
                                       var_name,
                                       help_message));
    }

    this->nreq_remaining += (required ? 1 : 0);
    this->arguments.emplace_back(short_argument,
                                 long_argument,
                                 required,
                                 type,
                                 var,
                                 var_size,
                                 var_name,
                                 help_message);
}

}  // namespace args
