#ifndef WECHAT_BACKTRACE_PREDEFINED_H
#define WECHAT_BACKTRACE_PREDEFINED_H

#define DEFINE_STATIC_LOCAL(type, name, arguments) \
  static type& name = *new type arguments

#define DEFINE_STATIC_CPP_FIELD(type, name, arguments) \
  type& name = *new type arguments

#define BACKTRACE_EXPORT __attribute__ ((visibility ("default")))

#define BACKTRACE_FUNC_WRAPPER(fn) fn

#endif //WECHAT_BACKTRACE_PREDEFINED_H
