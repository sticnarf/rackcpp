#ifndef HANE_HELLO_HTML_HPP
#define HANE_HELLO_HTML_HPP

#include <hane/hane.hpp>

class HelloHtml : public Template {
    std::string name;
protected:
    std::string render() override;
    friend class HelloMiddleware;
};

#endif