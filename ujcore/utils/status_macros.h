#pragma once

#include "absl/status/status.h"  // IWYU pragma: keep
#include "absl/status/statusor.h"  // IWYU pragma: keep

// Taken from: https://github.com/abseil/abseil-cpp/issues/976

// Run a command that returns a absl::Status.  If the called code returns an
// error status, return that status up out of this method too.
//
// Example:
//   RETURN_IF_ERROR(DoThings(4));
#define RETURN_IF_ERROR(expr)                                                \
  do {                                                                       \
    /* Using _status below to avoid capture problems if expr is "status". */ \
    const absl::Status _status = (expr);                                     \
    if (_PREDICT_FALSE(!_status.ok())) return _status;                       \
  } while (0)

// Run a command that returns a absl::StatusOr<T>.  If the called code returns
// an error status, return that status up out of this method too.
//
// Example:
//   ASSIGN_OR_RETURN(auto value, MaybeGetValue(arg));
#define ASSIGN_OR_RETURN(...)                                \
  STATUS_MACROS_IMPL_GET_VARIADIC_(                          \
      (__VA_ARGS__, STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_2_)) \
  (__VA_ARGS__)


#define CAT2(X,Y) X##Y
#define CAT(X,Y) CAT2(X,Y)



// Stringize the argument after it has been expanded
#define STRINGIZE_DETAIL(x) #x

// Force expansion of the argument (e.g., __LINE__) before stringizing
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

// Concatenate the file name, a separator, and the stringized line number
#define FILE_LINE __FILE__ ":" STRINGIZE(__LINE__)

#define RETURN_IF_NOT_FOUND_IN_MAP(lhs, store, key)                            \
    auto CAT(iter, __LINE__) = (store).find(key);                              \
    if (_PREDICT_FALSE(CAT(iter, __LINE__) == (store).end())) {                \
      return absl::NotFoundError("Key not found: " + std::string(FILE_LINE));     \
    }                                                                          \
    lhs = CAT(iter, __LINE__)->second;                                         \


// =================================================================
// == Implementation details, do not rely on anything below here. ==
// =================================================================

#define STATUS_MACROS_IMPL_GET_VARIADIC_HELPER_(_1, _2, NAME, ...) NAME
#define STATUS_MACROS_IMPL_GET_VARIADIC_(args) \
  STATUS_MACROS_IMPL_GET_VARIADIC_HELPER_ args

#define STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_2_(lhs, rexpr)                     \
  STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_(                                        \
      STATUS_MACROS_IMPL_CONCAT_(_status_or_value, __LINE__), lhs, rexpr,      \
      return std::move(STATUS_MACROS_IMPL_CONCAT_(_status_or_value, __LINE__)) \
          .status())

#define STATUS_MACROS_IMPL_ASSIGN_OR_RETURN_(statusor, lhs, rexpr, \
                                             error_expression)     \
  auto statusor = (rexpr);                                         \
  if (_PREDICT_FALSE(!statusor.ok())) {                            \
    error_expression;                                              \
  }                                                                \
  lhs = std::move(statusor).value()

// Internal helper for concatenating macro values.
#define STATUS_MACROS_IMPL_CONCAT_INNER_(x, y) x##y
#define STATUS_MACROS_IMPL_CONCAT_(x, y) STATUS_MACROS_IMPL_CONCAT_INNER_(x, y)

// Internal helper for stringifying macro values.
#define _PREDICT_FALSE(x) (__builtin_expect(false || (x), false))
