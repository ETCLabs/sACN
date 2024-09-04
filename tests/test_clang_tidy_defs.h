/******************************************************************************
 * Copyright 2024 ETC Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************
 * This file is a part of sACN. For more information, go to:
 * https://github.com/ETCLabs/sACN
 *****************************************************************************/

// Definitions to make unit tests pass static analysis more easily.

#ifndef TESTS_TEST_CLANG_TIDY_DEFS_H_
#define TESTS_TEST_CLANG_TIDY_DEFS_H_

// GoogleTest's macro expansions do some things that Clang Tidy doesn't like, so we work around
// them here using NOLINT directives.

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-macro-usage,cppcoreguidelines-owning-memory)
#define TIDY_TEST(test_suite_name, test_name) TEST(test_suite_name, test_name)
#define TIDY_TEST_F(test_fixture, test_name)  TEST_F(test_fixture, test_name)
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-macro-usage,cppcoreguidelines-owning-memory)

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,cppcoreguidelines-avoid-goto)
#define TIDY_EXPECT_NO_THROW(statement) EXPECT_NO_THROW(statement)

// NOLINTBEGIN(readability-redundant-string-init,cppcoreguidelines-avoid-const-or-ref-data-members)
#define TIDY_MATCHER_P(name, parameter, description) MATCHER_P(name, parameter, description)
#define TIDY_MATCHER_P2(name, parameter_1, parameter_2, description) \
  MATCHER_P2(name, parameter_1, parameter_2, description)
// NOLINTEND(readability-redundant-string-init,cppcoreguidelines-avoid-const-or-ref-data-members)

#endif  // TESTS_TEST_CLANG_TIDY_DEFS_H_
