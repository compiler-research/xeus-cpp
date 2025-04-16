/************************************************************************************
 * Copyright (c) 2023, xeus-cpp contributors                                        *
 *                                                                                  *
 * Distributed under the terms of the BSD 3-Clause License.                         *
 *                                                                                  *
 * The full license is in the file LICENSE, distributed with this software.         *
 ************************************************************************************/

#include "execution.hpp"
#include "xeus-cpp/xinterpreter.hpp"

#include "../xparser.hpp"
#include "clang/Interpreter/CppInterOp.h"

namespace xcpp
{
    struct StreamRedirectRAII
    {
        std::string& err;

        StreamRedirectRAII(std::string& e)
            : err(e)
        {
            Cpp::BeginStdStreamCapture(Cpp::kStdErr);
            Cpp::BeginStdStreamCapture(Cpp::kStdOut);
        }

        ~StreamRedirectRAII()
        {
            std::string out = Cpp::EndStdStreamCapture();
            err = Cpp::EndStdStreamCapture();
        }
    };

    timeit::timeit(Cpp::TInterp_t p)
        : m_interpreter(p)
    {
        bool compilation_result = true;
        std::string err;
        compilation_result = Cpp::Process("#include <chrono>\n#include <iostream>\n");

        // Define the reusable timing function once
        std::string timing_function = R"(
             double get_elapsed_time(std::size_t num_iterations, void (*func)()) {
                 auto _t2 = std::chrono::high_resolution_clock::now();
                 for (std::size_t _i = 0; _i < num_iterations; ++_i) {
                     func();
                 }
                 auto _t3 = std::chrono::high_resolution_clock::now();
                 return std::chrono::duration_cast<std::chrono::microseconds>(_t3 - _t2).count();
             }
         )";
        compilation_result = Cpp::Process(timing_function.c_str());
    }

    void timeit::get_options(argparser& argpars)
    {
        argpars.add_description("Time execution of a C++ statement or expression");
        argpars.add_argument("-n", "--number")
            .help("execute the given statement n times in a loop. If this value is not given, a fitting value is chosen"
            )
            .default_value(0)
            .scan<'i', int>();
        argpars.add_argument("-r", "--repeat")
            .help("repeat the loop iteration r times and take the best result")
            .default_value(7)
            .scan<'i', int>();
        argpars.add_argument("-p", "--precision")
            .help("use a precision of p digits to display the timing result")
            .default_value(3)
            .scan<'i', int>();
        argpars.add_argument("expression").help("expression to be evaluated").remaining();
        argpars.add_argument("-h", "--help")
            .action(
                [&](const std::string& /*unused*/)
                {
                    std::cout << argpars.help().str();
                }
            )
            .default_value(false)
            .help("shows help message")
            .implicit_value(true)
            .nargs(0);
    }

    std::string timeit::inner(std::size_t number, const std::string& code) const
    {
        static std::size_t counter = 0;  // Ensure unique lambda names
        std::string unique_id = std::to_string(counter++);
        std::string timeit_code = "";
        timeit_code += "auto user_code_" + unique_id + " = []() {\n";
        timeit_code += "   " + code + "\n";
        timeit_code += "};\n";
        timeit_code += "get_elapsed_time(" + std::to_string(number) + ", user_code_" + unique_id + ")\n";
        return timeit_code;
    }

    std::string timeit::_format_time(double timespan, std::size_t precision) const
    {
        std::vector<std::string> units{"s", "ms", "us", "ns"};
        std::vector<double> scaling{1, 1e3, 1e6, 1e9};
        std::ostringstream output;
        int order;

        if (timespan > 0.0)
        {
            order = std::min(-static_cast<int>(std::floor(std::floor(std::log10(timespan)) / 3)), 3);
        }
        else
        {
            order = 3;
        }
        output.precision(precision);
        output << timespan * scaling[order] << " " << units[order];
        return output.str();
    }

    void timeit::execute(std::string& line, std::string& cell)
    {
        argparser argpars("timeit", XEUS_CPP_VERSION, argparse::default_arguments::none);
        get_options(argpars);
        argpars.parse(line);

        int number = argpars.get<int>("-n");
        int repeat = argpars.get<int>("-r");
        int precision = argpars.get<int>("-p");

        std::string code;
        try
        {
            const auto& v = argpars.get<std::vector<std::string>>("expression");
            for (const auto& s : v)
            {
                code += " " + s;
            }
        }
        catch (std::logic_error& e)
        {
            if (trim(cell).empty() && (argpars["-h"] == false))
            {
                std::cerr << "No expression given to evaluate" << std::endl;
            }
        }

        code += cell;
        if (trim(code).empty())
        {
            return;
        }

        auto errorlevel = 0;
        std::string ename;
        std::string evalue;
        std::string output;
        std::string err;
        bool hadError = false;

        try
        {
            if (number == 0)
            {
                for (std::size_t n = 0; n < 10; ++n)
                {
                    number = std::pow(10, n);
                    std::string timeit_code = inner(number, code);
                    std::ostringstream buffer_out, buffer_err;
                    std::streambuf* old_cout = std::cout.rdbuf(buffer_out.rdbuf());
                    std::streambuf* old_cerr = std::cerr.rdbuf(buffer_err.rdbuf());
                    StreamRedirectRAII R(err);
                    auto res_ptr = Cpp::Evaluate(timeit_code.c_str(), &hadError);
                    std::cout.rdbuf(old_cout);
                    std::cerr.rdbuf(old_cerr);
                    output = std::to_string(res_ptr);
                    err += buffer_err.str();
                    double elapsed_time = std::stod(output) * 1e-6;
                    if (elapsed_time >= 0.2)
                    {
                        break;
                    }
                }
            }

            std::vector<double> all_runs;
            double mean = 0;
            double stdev = 0;
            for (std::size_t r = 0; r < static_cast<std::size_t>(repeat); ++r)
            {
                std::string timeit_code = inner(number, code);
                std::ostringstream buffer_out, buffer_err;
                std::streambuf* old_cout = std::cout.rdbuf(buffer_out.rdbuf());
                std::streambuf* old_cerr = std::cerr.rdbuf(buffer_err.rdbuf());
                StreamRedirectRAII R(err);
                auto res_ptr = Cpp::Evaluate(timeit_code.c_str(), &hadError);
                std::cout.rdbuf(old_cout);
                std::cerr.rdbuf(old_cerr);
                output = std::to_string(res_ptr);
                err += buffer_err.str();
                double elapsed_time = std::stod(output) * 1e-6;
                all_runs.push_back(elapsed_time / number);
                mean += all_runs.back();
            }
            mean /= repeat;
            for (std::size_t r = 0; r < static_cast<std::size_t>(repeat); ++r)
            {
                stdev += (all_runs[r] - mean) * (all_runs[r] - mean);
            }
            stdev = std::sqrt(stdev / repeat);

            std::cout << _format_time(mean, precision) << " +- " << _format_time(stdev, precision);
            std::cout << " per loop (mean +- std. dev. of " << repeat << " run"
                      << ((repeat == 1) ? ", " : "s ");
            std::cout << number << " loop" << ((number == 1) ? "" : "s") << " each)" << std::endl;
        }
        catch (std::exception& e)
        {
            errorlevel = 1;
            ename = "Standard Exception: ";
            evalue = e.what();
        }
        catch (...)
        {
            errorlevel = 1;
            ename = "Error: ";
        }

        if (hadError)
        {
            errorlevel = 1;
            ename = "Error: ";
            evalue = "Compilation error! " + err;
            std::cerr << err;
            std::cerr << "Error: " << evalue << std::endl;
            std::cerr << "Error: " << err << std::endl;
        }
    }
}