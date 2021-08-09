/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>

#include <gtest/gtest.h>

#include <demangle.h>

#include "Demangler.h"

TEST(DemangleTest, IllegalArgumentModifiers) {
  Demangler demangler;

  ASSERT_EQ("_Zpp4FUNKK", demangler.Parse("_Zpp4FUNKK"));
  ASSERT_EQ("_Zpp4FUNVV", demangler.Parse("_Zpp4FUNVV"));
}

TEST(DemangleTest, VoidArgument) {
  Demangler demangler;

  ASSERT_EQ("func()", demangler.Parse("_ZN4funcEv"));
  ASSERT_EQ("func(void&)", demangler.Parse("_ZN4funcERv"));
  ASSERT_EQ("func(void, void)", demangler.Parse("_ZN4funcEvv"));
  ASSERT_EQ("func(void*)", demangler.Parse("_ZN4funcEPv"));
  ASSERT_EQ("func(void const)", demangler.Parse("_ZN4funcEKv"));
  ASSERT_EQ("func(void volatile)", demangler.Parse("_ZN4funcEVv"));
}

TEST(DemangleTest, ArgumentModifiers) {
  Demangler demangler;

  ASSERT_EQ("func(char)", demangler.Parse("_ZN4funcEc"));
  ASSERT_EQ("func(char*)", demangler.Parse("_ZN4funcEPc"));
  ASSERT_EQ("func(char**)", demangler.Parse("_ZN4funcEPPc"));
  ASSERT_EQ("func(char***)", demangler.Parse("_ZN4funcEPPPc"));
  ASSERT_EQ("func(char&)", demangler.Parse("_ZN4funcERc"));
  ASSERT_EQ("func(char*&)", demangler.Parse("_ZN4funcERPc"));
  ASSERT_EQ("func(char&)", demangler.Parse("_ZN4funcERRc"));
  ASSERT_EQ("func(char*&*)", demangler.Parse("_ZN4funcEPRPc"));
  ASSERT_EQ("func(char**&)", demangler.Parse("_ZN4funcERRPPc"));
  ASSERT_EQ("func(char const)", demangler.Parse("_ZN4funcEKc"));
  ASSERT_EQ("func(char volatile)", demangler.Parse("_ZN4funcEVc"));
  ASSERT_EQ("func(char volatile const)", demangler.Parse("_ZN4funcEKVc"));
  ASSERT_EQ("func(char const volatile)", demangler.Parse("_ZN4funcEVKc"));
  ASSERT_EQ("func(char const* volatile&)", demangler.Parse("_ZN4funcERVPKc"));
  ASSERT_EQ("func(void, char, short)", demangler.Parse("_ZN4funcEvcs"));
  ASSERT_EQ("func(void*, char&, short&*)", demangler.Parse("_ZN4funcEPvRcPRs"));
}

TEST(DemangleTest, FunctionModifiers) {
  Demangler demangler;

  ASSERT_EQ("func() const", demangler.Parse("_ZNK4funcEv"));
  ASSERT_EQ("func() volatile", demangler.Parse("_ZNV4funcEv"));
  ASSERT_EQ("func() volatile const", demangler.Parse("_ZNKV4funcEv"));
  ASSERT_EQ("func() const volatile", demangler.Parse("_ZNVK4funcEv"));
}

TEST(DemangleTest, MultiplePartsInName) {
  Demangler demangler;

  ASSERT_EQ("one::two()", demangler.Parse("_ZN3one3twoEv"));
  ASSERT_EQ("one::two::three()", demangler.Parse("_ZN3one3two5threeEv"));
  ASSERT_EQ("one::two::three::four()", demangler.Parse("_ZN3one3two5three4fourEv"));
  ASSERT_EQ("one::two::three::four::five()", demangler.Parse("_ZN3one3two5three4four4fiveEv"));
  ASSERT_EQ("one(two::three::four::five)", demangler.Parse("_ZN3oneEN3two5three4four4fiveE"));
}

TEST(DemangleTest, AnonymousNamespace) {
  Demangler demangler;

  ASSERT_EQ("(anonymous namespace)::two()", demangler.Parse("_ZN12_GLOBAL__N_13twoEv"));
  ASSERT_EQ("one::two((anonymous namespace))", demangler.Parse("_ZN3one3twoE12_GLOBAL__N_1"));
}

TEST(DemangleTest, DestructorValues) {
  Demangler demangler;

  ASSERT_EQ("one::two::~two()", demangler.Parse("_ZN3one3twoD0Ev"));
  ASSERT_EQ("one::two::~two()", demangler.Parse("_ZN3one3twoD1Ev"));
  ASSERT_EQ("one::two::~two()", demangler.Parse("_ZN3one3twoD2Ev"));
  ASSERT_EQ("one::two::~two()", demangler.Parse("_ZN3one3twoD5Ev"));
  ASSERT_EQ("one::two::three::~three()", demangler.Parse("_ZN3one3two5threeD0Ev"));

  ASSERT_EQ("_ZN3one3twoD3Ev", demangler.Parse("_ZN3one3twoD3Ev"));
  ASSERT_EQ("_ZN3one3twoD4Ev", demangler.Parse("_ZN3one3twoD4Ev"));
  ASSERT_EQ("_ZN3one3twoD6Ev", demangler.Parse("_ZN3one3twoD6Ev"));
  ASSERT_EQ("_ZN3one3twoD7Ev", demangler.Parse("_ZN3one3twoD7Ev"));
  ASSERT_EQ("_ZN3one3twoD8Ev", demangler.Parse("_ZN3one3twoD8Ev"));
  ASSERT_EQ("_ZN3one3twoD9Ev", demangler.Parse("_ZN3one3twoD9Ev"));

  ASSERT_EQ("one::two<three::four>::~two()", demangler.Parse("_ZN3one3twoIN5three4fourEED2Ev"));
}

TEST(DemangleTest, ConstructorValues) {
  Demangler demangler;

  ASSERT_EQ("one::two::two()", demangler.Parse("_ZN3one3twoC1Ev"));
  ASSERT_EQ("one::two::two()", demangler.Parse("_ZN3one3twoC2Ev"));
  ASSERT_EQ("one::two::two()", demangler.Parse("_ZN3one3twoC3Ev"));
  ASSERT_EQ("one::two::two()", demangler.Parse("_ZN3one3twoC5Ev"));
  ASSERT_EQ("one::two::three::three()", demangler.Parse("_ZN3one3two5threeC1Ev"));

  ASSERT_EQ("_ZN3one3twoC0Ev", demangler.Parse("_ZN3one3twoC0Ev"));
  ASSERT_EQ("_ZN3one3twoC4Ev", demangler.Parse("_ZN3one3twoC4Ev"));
  ASSERT_EQ("_ZN3one3twoC6Ev", demangler.Parse("_ZN3one3twoC6Ev"));
  ASSERT_EQ("_ZN3one3twoC7Ev", demangler.Parse("_ZN3one3twoC7Ev"));
  ASSERT_EQ("_ZN3one3twoC8Ev", demangler.Parse("_ZN3one3twoC8Ev"));
  ASSERT_EQ("_ZN3one3twoC9Ev", demangler.Parse("_ZN3one3twoC9Ev"));

  ASSERT_EQ("one::two<three::four>::two()", demangler.Parse("_ZN3one3twoIN5three4fourEEC1Ev"));
}

TEST(DemangleTest, OperatorValues) {
  Demangler demangler;

  ASSERT_EQ("operator&&()", demangler.Parse("_Zaav"));
  ASSERT_EQ("operator&()", demangler.Parse("_Zadv"));
  ASSERT_EQ("operator&()", demangler.Parse("_Zanv"));
  ASSERT_EQ("operator&=()", demangler.Parse("_ZaNv"));
  ASSERT_EQ("operator=()", demangler.Parse("_ZaSv"));
  ASSERT_EQ("operator()()", demangler.Parse("_Zclv"));
  ASSERT_EQ("operator,()", demangler.Parse("_Zcmv"));
  ASSERT_EQ("operator~()", demangler.Parse("_Zcov"));
  ASSERT_EQ("operator delete[]()", demangler.Parse("_Zdav"));
  ASSERT_EQ("operator*()", demangler.Parse("_Zdev"));
  ASSERT_EQ("operator delete()", demangler.Parse("_Zdlv"));
  ASSERT_EQ("operator/()", demangler.Parse("_Zdvv"));
  ASSERT_EQ("operator/=()", demangler.Parse("_ZdVv"));
  ASSERT_EQ("operator^()", demangler.Parse("_Zeov"));
  ASSERT_EQ("operator^=()", demangler.Parse("_ZeOv"));
  ASSERT_EQ("operator==()", demangler.Parse("_Zeqv"));
  ASSERT_EQ("operator>=()", demangler.Parse("_Zgev"));
  ASSERT_EQ("operator>()", demangler.Parse("_Zgtv"));
  ASSERT_EQ("operator[]()", demangler.Parse("_Zixv"));
  ASSERT_EQ("operator<=()", demangler.Parse("_Zlev"));
  ASSERT_EQ("operator<<()", demangler.Parse("_Zlsv"));
  ASSERT_EQ("operator<<=()", demangler.Parse("_ZlSv"));
  ASSERT_EQ("operator<()", demangler.Parse("_Zltv"));
  ASSERT_EQ("operator-()", demangler.Parse("_Zmiv"));
  ASSERT_EQ("operator-=()", demangler.Parse("_ZmIv"));
  ASSERT_EQ("operator*()", demangler.Parse("_Zmlv"));
  ASSERT_EQ("operator*=()", demangler.Parse("_ZmLv"));
  ASSERT_EQ("operator--()", demangler.Parse("_Zmmv"));
  ASSERT_EQ("operator new[]()", demangler.Parse("_Znav"));
  ASSERT_EQ("operator!=()", demangler.Parse("_Znev"));
  ASSERT_EQ("operator-()", demangler.Parse("_Zngv"));
  ASSERT_EQ("operator!()", demangler.Parse("_Zntv"));
  ASSERT_EQ("operator new()", demangler.Parse("_Znwv"));
  ASSERT_EQ("operator||()", demangler.Parse("_Zoov"));
  ASSERT_EQ("operator|()", demangler.Parse("_Zorv"));
  ASSERT_EQ("operator|=()", demangler.Parse("_ZoRv"));
  ASSERT_EQ("operator->*()", demangler.Parse("_Zpmv"));
  ASSERT_EQ("operator+()", demangler.Parse("_Zplv"));
  ASSERT_EQ("operator+=()", demangler.Parse("_ZpLv"));
  ASSERT_EQ("operator++()", demangler.Parse("_Zppv"));
  ASSERT_EQ("operator+()", demangler.Parse("_Zpsv"));
  ASSERT_EQ("operator->()", demangler.Parse("_Zptv"));
  ASSERT_EQ("operator?()", demangler.Parse("_Zquv"));
  ASSERT_EQ("operator%()", demangler.Parse("_Zrmv"));
  ASSERT_EQ("operator%=()", demangler.Parse("_ZrMv"));
  ASSERT_EQ("operator>>()", demangler.Parse("_Zrsv"));
  ASSERT_EQ("operator>>=()", demangler.Parse("_ZrSv"));

  // Spot check using an operator as part of function name.
  ASSERT_EQ("operator&&()", demangler.Parse("_ZNaaEv"));
  ASSERT_EQ("operator++()", demangler.Parse("_ZNppEv"));
  ASSERT_EQ("one::operator++()", demangler.Parse("_ZN3oneppEv"));

  // Spot check using an operator in an argument name.
  ASSERT_EQ("operator+(operator|=)", demangler.Parse("_ZNpsENoRE"));
  ASSERT_EQ("operator==()", demangler.Parse("_Zeqv"));
  ASSERT_EQ("one(arg1::operator|=, arg2::operator==)",
            demangler.Parse("_ZN3oneEN4arg1oREN4arg2eqE"));
}

TEST(DemangleTest, FunctionStartsWithNumber) {
  Demangler demangler;

  ASSERT_EQ("value(char, int)", demangler.Parse("_Z5valueci"));
  ASSERT_EQ("abcdefjklmn(signed char)", demangler.Parse("_Z11abcdefjklmna"));
  ASSERT_EQ("value(one, signed char)", demangler.Parse("_Z5value3onea"));
}

TEST(DemangleTest, FunctionStartsWithLPlusNumber) {
  Demangler demangler;

  ASSERT_EQ("value(char, int)", demangler.Parse("_ZL5valueci"));
  ASSERT_EQ("abcdefjklmn(signed char)", demangler.Parse("_ZL11abcdefjklmna"));
  ASSERT_EQ("value(one, signed char)", demangler.Parse("_ZL5value3onea"));
}

TEST(DemangleTest, StdTypes) {
  Demangler demangler;

  ASSERT_EQ("std::one", demangler.Parse("_ZNSt3oneE"));
  ASSERT_EQ("std::one(std::two)", demangler.Parse("_ZNSt3oneESt3two"));
  ASSERT_EQ("std::std::one(std::two)", demangler.Parse("_ZNStSt3oneESt3two"));
  ASSERT_EQ("std()", demangler.Parse("_ZNStEv"));
  ASSERT_EQ("one::std::std::two::~two(one::std::std::two)",
            demangler.Parse("_ZN3oneStSt3twoD0ES0_"));

  ASSERT_EQ("std::allocator", demangler.Parse("_ZNSaE"));
  ASSERT_EQ("std::basic_string", demangler.Parse("_ZNSbE"));
  ASSERT_EQ("_ZNScE", demangler.Parse("_ZNScE"));
  ASSERT_EQ("std::iostream", demangler.Parse("_ZNSdE"));
  ASSERT_EQ("_ZNSeE", demangler.Parse("_ZNSeE"));
  ASSERT_EQ("_ZNSfE", demangler.Parse("_ZNSfE"));
  ASSERT_EQ("_ZNSgE", demangler.Parse("_ZNSgE"));
  ASSERT_EQ("_ZNShE", demangler.Parse("_ZNShE"));
  ASSERT_EQ("std::istream", demangler.Parse("_ZNSiE"));
  ASSERT_EQ("_ZNSjE", demangler.Parse("_ZNSjE"));
  ASSERT_EQ("_ZNSkE", demangler.Parse("_ZNSkE"));
  ASSERT_EQ("_ZNSlE", demangler.Parse("_ZNSlE"));
  ASSERT_EQ("_ZNSmE", demangler.Parse("_ZNSmE"));
  ASSERT_EQ("_ZNSnE", demangler.Parse("_ZNSnE"));
  ASSERT_EQ("std::ostream", demangler.Parse("_ZNSoE"));
  ASSERT_EQ("_ZNSpE", demangler.Parse("_ZNSpE"));
  ASSERT_EQ("_ZNSqE", demangler.Parse("_ZNSqE"));
  ASSERT_EQ("_ZNSrE", demangler.Parse("_ZNSrE"));
  ASSERT_EQ("std::string", demangler.Parse("_ZNSsE"));
  ASSERT_EQ("_ZNSuE", demangler.Parse("_ZNSuE"));
  ASSERT_EQ("_ZNSvE", demangler.Parse("_ZNSvE"));
  ASSERT_EQ("_ZNSwE", demangler.Parse("_ZNSwE"));
  ASSERT_EQ("_ZNSxE", demangler.Parse("_ZNSxE"));
  ASSERT_EQ("_ZNSyE", demangler.Parse("_ZNSyE"));
  ASSERT_EQ("_ZNSzE", demangler.Parse("_ZNSzE"));
}

TEST(DemangleTest, SingleLetterArguments) {
  Demangler demangler;

  ASSERT_EQ("func(signed char)", demangler.Parse("_ZN4funcEa"));
  ASSERT_EQ("func(bool)", demangler.Parse("_ZN4funcEb"));
  ASSERT_EQ("func(char)", demangler.Parse("_ZN4funcEc"));
  ASSERT_EQ("func(double)", demangler.Parse("_ZN4funcEd"));
  ASSERT_EQ("func(long double)", demangler.Parse("_ZN4funcEe"));
  ASSERT_EQ("func(float)", demangler.Parse("_ZN4funcEf"));
  ASSERT_EQ("func(__float128)", demangler.Parse("_ZN4funcEg"));
  ASSERT_EQ("func(unsigned char)", demangler.Parse("_ZN4funcEh"));
  ASSERT_EQ("func(int)", demangler.Parse("_ZN4funcEi"));
  ASSERT_EQ("func(unsigned int)", demangler.Parse("_ZN4funcEj"));
  ASSERT_EQ("_ZN4funcEk", demangler.Parse("_ZN4funcEk"));
  ASSERT_EQ("func(long)", demangler.Parse("_ZN4funcEl"));
  ASSERT_EQ("func(unsigned long)", demangler.Parse("_ZN4funcEm"));
  ASSERT_EQ("func(__int128)", demangler.Parse("_ZN4funcEn"));
  ASSERT_EQ("func(unsigned __int128)", demangler.Parse("_ZN4funcEo"));
  ASSERT_EQ("_ZN4funcEp", demangler.Parse("_ZN4funcEp"));
  ASSERT_EQ("_ZN4funcEq", demangler.Parse("_ZN4funcEq"));
  ASSERT_EQ("_ZN4funcEr", demangler.Parse("_ZN4funcEr"));
  ASSERT_EQ("func(short)", demangler.Parse("_ZN4funcEs"));
  ASSERT_EQ("func(unsigned short)", demangler.Parse("_ZN4funcEt"));
  ASSERT_EQ("_ZN4funcEu", demangler.Parse("_ZN4funcEu"));
  ASSERT_EQ("func()", demangler.Parse("_ZN4funcEv"));
  ASSERT_EQ("func(wchar_t)", demangler.Parse("_ZN4funcEw"));
  ASSERT_EQ("func(long long)", demangler.Parse("_ZN4funcEx"));
  ASSERT_EQ("func(unsigned long long)", demangler.Parse("_ZN4funcEy"));
  ASSERT_EQ("func(...)", demangler.Parse("_ZN4funcEz"));
}

TEST(DemangleTest, DArguments) {
  Demangler demangler;

  ASSERT_EQ("func(auto)", demangler.Parse("_ZN4funcEDa"));
  ASSERT_EQ("_ZN4funcEDb", demangler.Parse("_ZN4funcEDb"));
  ASSERT_EQ("_ZN4funcEDc", demangler.Parse("_ZN4funcEDc"));
  ASSERT_EQ("func(decimal64)", demangler.Parse("_ZN4funcEDd"));
  ASSERT_EQ("func(decimal128)", demangler.Parse("_ZN4funcEDe"));
  ASSERT_EQ("func(decimal32)", demangler.Parse("_ZN4funcEDf"));
  ASSERT_EQ("_ZN4funcEDg", demangler.Parse("_ZN4funcEDg"));
  ASSERT_EQ("func(half)", demangler.Parse("_ZN4funcEDh"));
  ASSERT_EQ("func(char32_t)", demangler.Parse("_ZN4funcEDi"));
  ASSERT_EQ("_ZN4funcEDj", demangler.Parse("_ZN4funcEDj"));
  ASSERT_EQ("_ZN4funcEDk", demangler.Parse("_ZN4funcEDk"));
  ASSERT_EQ("_ZN4funcEDl", demangler.Parse("_ZN4funcEDl"));
  ASSERT_EQ("_ZN4funcEDm", demangler.Parse("_ZN4funcEDm"));
  ASSERT_EQ("func(decltype(nullptr))", demangler.Parse("_ZN4funcEDn"));
  ASSERT_EQ("_ZN4funcEDo", demangler.Parse("_ZN4funcEDo"));
  ASSERT_EQ("_ZN4funcEDp", demangler.Parse("_ZN4funcEDp"));
  ASSERT_EQ("_ZN4funcEDq", demangler.Parse("_ZN4funcEDq"));
  ASSERT_EQ("_ZN4funcEDr", demangler.Parse("_ZN4funcEDr"));
  ASSERT_EQ("func(char16_t)", demangler.Parse("_ZN4funcEDs"));
  ASSERT_EQ("_ZN4funcEDt", demangler.Parse("_ZN4funcEDt"));
  ASSERT_EQ("_ZN4funcEDu", demangler.Parse("_ZN4funcEDu"));
  ASSERT_EQ("_ZN4funcEDv", demangler.Parse("_ZN4funcEDv"));
  ASSERT_EQ("_ZN4funcEDw", demangler.Parse("_ZN4funcEDw"));
  ASSERT_EQ("_ZN4funcEDx", demangler.Parse("_ZN4funcEDx"));
  ASSERT_EQ("_ZN4funcEDy", demangler.Parse("_ZN4funcEDy"));
  ASSERT_EQ("_ZN4funcEDz", demangler.Parse("_ZN4funcEDz"));
}

TEST(DemangleTest, FunctionArguments) {
  Demangler demangler;

  ASSERT_EQ("func(char ())", demangler.Parse("_ZN4funcEFcvE"));
  ASSERT_EQ("func(char (*)())", demangler.Parse("_ZN4funcEPFcvE"));
  ASSERT_EQ("func(char (&)())", demangler.Parse("_ZN4funcERFcvE"));
  ASSERT_EQ("func(char (&)())", demangler.Parse("_ZN4funcERFcvE"));
  ASSERT_EQ("func(char (*&)())", demangler.Parse("_ZN4funcERPFcvE"));
  ASSERT_EQ("func(char (*)(int) const)", demangler.Parse("_ZN4funcEPKFciE"));
  ASSERT_EQ("func(char (&)() const)", demangler.Parse("_ZN4funcERKFcvE"));
  ASSERT_EQ("func(char (&)() volatile)", demangler.Parse("_ZN4funcERVFcvE"));
  ASSERT_EQ("func(char (&)() volatile const)", demangler.Parse("_ZN4funcERKVFcvE"));
  ASSERT_EQ("func(char (&)() const volatile)", demangler.Parse("_ZN4funcERVKFcvE"));
  ASSERT_EQ("func(char (&)(int, signed char) const)", demangler.Parse("_ZN4funcERKFciaE"));
  ASSERT_EQ("fake(char (&* volatile const)(void, void, signed char), signed char)",
            demangler.Parse("_ZN4fakeEKVPRFcvvaEa"));
}

TEST(DemangleTest, TemplateFunction) {
  Demangler demangler;

  ASSERT_EQ("one<char>", demangler.Parse("_ZN3oneIcEE"));
  ASSERT_EQ("one<void>", demangler.Parse("_ZN3oneIvEE"));
  ASSERT_EQ("one<void*>", demangler.Parse("_ZN3oneIPvEE"));
  ASSERT_EQ("one<void const>", demangler.Parse("_ZN3oneIKvEE"));
  ASSERT_EQ("one<char, int, bool>", demangler.Parse("_ZN3oneIcibEE"));
  ASSERT_EQ("one::two<three>", demangler.Parse("_ZN3one3twoIN5threeEEE"));
  ASSERT_EQ("one<char, int, two::three>", demangler.Parse("_ZN3oneIciN3two5threeEEE"));
  // Template within templates.
  ASSERT_EQ("one::two<three<char, int>>", demangler.Parse("_ZN3one3twoIN5threeIciEEEE"));
  ASSERT_EQ("one::two<three<char, four<int>>>", demangler.Parse("_ZN3one3twoIN5threeIcN4fourIiEEEEEE"));

  ASSERT_EQ("one<char>", demangler.Parse("_Z3oneIcE"));
  ASSERT_EQ("one<void>", demangler.Parse("_Z3oneIvE"));
  ASSERT_EQ("one<void*>", demangler.Parse("_Z3oneIPvE"));
  ASSERT_EQ("one<void const>", demangler.Parse("_Z3oneIKvE"));
  ASSERT_EQ("one<char, int, bool>", demangler.Parse("_Z3oneIcibE"));
  ASSERT_EQ("one(two<three>)", demangler.Parse("_Z3one3twoIN5threeEE"));
  ASSERT_EQ("one<char, int, two::three>", demangler.Parse("_Z3oneIciN3two5threeEE"));
  // Template within templates.
  ASSERT_EQ("one(two<three<char, int>>)", demangler.Parse("_Z3one3twoIN5threeIciEEE"));
  ASSERT_EQ("one(two<three<char, four<int>>>)",
            demangler.Parse("_Z3one3twoIN5threeIcN4fourIiEEEEE"));
}

TEST(DemangleTest, TemplateFunctionWithReturnType) {
  Demangler demangler;

  ASSERT_EQ("char one<int>(char)", demangler.Parse("_Z3oneIiEcc"));
  ASSERT_EQ("void one<int>()", demangler.Parse("_Z3oneIiEvv"));
  ASSERT_EQ("char one<int>()", demangler.Parse("_Z3oneIiEcv"));
  ASSERT_EQ("char one<int>(void, void)", demangler.Parse("_Z3oneIiEcvv"));
  ASSERT_EQ("char one<int>()", demangler.Parse("_ZN3oneIiEEcv"));
  ASSERT_EQ("char one<int>(void, void)", demangler.Parse("_ZN3oneIiEEcvv"));
}

TEST(DemangleTest, TemplateArguments) {
  Demangler demangler;

  ASSERT_EQ("one(two<char>)", demangler.Parse("_ZN3oneE3twoIcE"));
  ASSERT_EQ("one(two<char, void>)", demangler.Parse("_ZN3oneE3twoIcvE"));
  ASSERT_EQ("one(two<char, void, three<four, int>>)",
            demangler.Parse("_ZN3oneE3twoIcv5threeI4fouriEE"));
}

TEST(DemangleTest, SubstitutionUnderscore) {
  Demangler demangler;

  ASSERT_EQ("a::a", demangler.Parse("_ZN1aS_E"));
  ASSERT_EQ("one::one", demangler.Parse("_ZN3oneS_E"));
  ASSERT_EQ("one::two::one", demangler.Parse("_ZN3one3twoS_E"));
  ASSERT_EQ("one::two::three::one", demangler.Parse("_ZN3one3two5threeS_E"));
  ASSERT_EQ("one::two(one)", demangler.Parse("_ZN3one3twoES_"));
  ASSERT_EQ("one::two(three::one)", demangler.Parse("_ZN3one3twoEN5threeS_E"));

  // Special case that St is part of the saved value used in the substitution.
  ASSERT_EQ("std::one::std::one", demangler.Parse("_ZNSt3oneS_E"));

  // Multiple substitutions in the string.
  ASSERT_EQ("one::one(one, one)", demangler.Parse("_ZN3oneS_ES_S_"));
  ASSERT_EQ("std::one::two::std::one(std::one)", demangler.Parse("_ZNSt3one3twoS_ES_"));
}

TEST(DemangleTest, SubstitutionByNumber) {
  Demangler demangler;

  // Basic substitution.
  ASSERT_EQ("a::b::c(a::b)", demangler.Parse("_ZN1a1b1cES0_"));
  ASSERT_EQ("_ZN1a1b1cES1_", demangler.Parse("_ZN1a1b1cES1_"));
  ASSERT_EQ("a::b::c::d(a::b::c)", demangler.Parse("_ZN1a1b1c1dES1_"));
  ASSERT_EQ("a::b::c::d::e::f::g::h::i::j::k::l::m::n::o::p::q(a::b::c::d::e::f::g::h::i::j::k::l)",
            demangler.Parse("_ZN1a1b1c1d1e1f1g1h1i1j1k1l1m1n1o1p1qESA_"));
  ASSERT_EQ("a::b::c::d::e::f::g::h::i::j::k::l::m::n::o::p::q(a::b::c::d::e::f::g::h::i::j::k::l::m)",
            demangler.Parse("_ZN1a1b1c1d1e1f1g1h1i1j1k1l1m1n1o1p1qESB_"));

  // Verify argument modifiers are included in substitution list.
  ASSERT_EQ("one::two(char&* volatile const, char&)", demangler.Parse("_ZN3one3twoEKVPRcS0_"));
  ASSERT_EQ("one::two(char&* volatile const, char&*)", demangler.Parse("_ZN3one3twoEKVPRcS1_"));
  ASSERT_EQ("one::two(char&* volatile const, char&* volatile const)",
            demangler.Parse("_ZN3one3twoEKVPRcS2_"));
  ASSERT_EQ("one::two(int&* volatile* const, int&)", demangler.Parse("_ZN3one3twoEKPVPRiS0_"));
  ASSERT_EQ("one::two(int&* volatile const, int&*)", demangler.Parse("_ZN3one3twoEKVPRiS1_"));
  ASSERT_EQ("one::two(int&* volatile const, int&* volatile const)",
            demangler.Parse("_ZN3one3twoEKVPRiS2_"));

  // Verify Constructor/Destructor does properly save from function name.
  ASSERT_EQ("_ZN1a1bES0_", demangler.Parse("_ZN1a1bES0_"));
  ASSERT_EQ("a::b::b(a::b)", demangler.Parse("_ZN1a1bC1ES0_"));
  ASSERT_EQ("a::b::~b(a::b)", demangler.Parse("_ZN1a1bD0ES0_"));

  // Make sure substitution values are not saved.
  ASSERT_EQ("a::b::b(a::b, char*, char*)", demangler.Parse("_ZN1a1bC1ES0_PcS1_"));
}

TEST(DemangleTest, ComplexSubstitution) {
  Demangler demangler;

  ASSERT_EQ("one::two<one::three>::two()", demangler.Parse("_ZN3one3twoINS_5threeEEC1Ev"));
  ASSERT_EQ("one::two::two(one::two const&, bool, one::three*)",
            demangler.Parse("_ZN3one3twoC2ERKS0_bPNS_5threeE"));
  ASSERT_EQ("one::two::three::four<one::five>::~four(one::two*)",
            demangler.Parse("_ZN3one3two5three4fourINS_4fiveEED2EPS0_"));
  ASSERT_EQ("one::two::three::four<one::five>::~four(one::two::three*)",
            demangler.Parse("_ZN3one3two5three4fourINS_4fiveEED2EPS1_"));
  ASSERT_EQ("one::two::three::four<one::five>::~four(one::two::three::four*)",
            demangler.Parse("_ZN3one3two5three4fourINS_4fiveEED2EPS2_"));
  ASSERT_EQ("one::two::three::four<one::five>::~four(one::five*)",
            demangler.Parse("_ZN3one3two5three4fourINS_4fiveEED2EPS3_"));
}

TEST(DemangleTest, TemplateSubstitution) {
  Demangler demangler;

  ASSERT_EQ("void one<int, double>(int)", demangler.Parse("_ZN3oneIidEEvT_"));
  ASSERT_EQ("void one<int, double>(double)", demangler.Parse("_ZN3oneIidEEvT0_"));
  ASSERT_EQ("void one<int, double, char, void>(char)", demangler.Parse("_ZN3oneIidcvEEvT1_"));

  ASSERT_EQ("void one<int, double>(int)", demangler.Parse("_Z3oneIidEvT_"));
  ASSERT_EQ("void one<int, double>(double)", demangler.Parse("_Z3oneIidEvT0_"));
  ASSERT_EQ("void one<int, double, char, void>(char)", demangler.Parse("_Z3oneIidcvEvT1_"));

  ASSERT_EQ("void one<a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r>(l)",
            demangler.Parse("_ZN3oneI1a1b1c1d1e1f1g1h1i1j1k1l1m1n1o1p1q1rEEvT10_"));
  ASSERT_EQ("void one<a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r>(m)",
            demangler.Parse("_ZN3oneI1a1b1c1d1e1f1g1h1i1j1k1l1m1n1o1p1q1rEEvT11_"));

  ASSERT_EQ("void one<a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r>(l)",
            demangler.Parse("_Z3oneI1a1b1c1d1e1f1g1h1i1j1k1l1m1n1o1p1q1rEvT10_"));
  ASSERT_EQ("void one<a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r>(m)",
            demangler.Parse("_Z3oneI1a1b1c1d1e1f1g1h1i1j1k1l1m1n1o1p1q1rEvT11_"));
}

TEST(DemangleTest, StringTooLong) {
  Demangler demangler;

  ASSERT_EQ("_ZN3one3twoC2ERKS0_bPNS_5threeE",
            demangler.Parse("_ZN3one3twoC2ERKS0_bPNS_5threeE", 10));
  ASSERT_EQ("_ZN3one3twoC2ERKS0_bPNS_5threeE",
            demangler.Parse("_ZN3one3twoC2ERKS0_bPNS_5threeE", 30));
  ASSERT_EQ("one::two::two(one::two const&, bool, one::three*)",
            demangler.Parse("_ZN3one3twoC2ERKS0_bPNS_5threeE", 31));

  // Check the length check only occurs after the two letter value
  // has been processed.
  ASSERT_EQ("one::two(auto)", demangler.Parse("_ZN3one3twoEDa", 15));
  ASSERT_EQ("one::two(auto)", demangler.Parse("_ZN3one3twoEDa", 14));
  ASSERT_EQ("one::two(auto)", demangler.Parse("_ZN3one3twoEDa", 13));
  ASSERT_EQ("_ZN3one3twoEDa", demangler.Parse("_ZN3one3twoEDa", 12));
}

TEST(DemangleTest, BooleanLiterals) {
  Demangler demangler;

  ASSERT_EQ("one<true>", demangler.Parse("_ZN3oneILb1EEE"));
  ASSERT_EQ("one<false>", demangler.Parse("_ZN3oneILb0EEE"));
  ASSERT_EQ("one<false, true>", demangler.Parse("_ZN3oneILb0ELb1EEE"));

  ASSERT_EQ("one<true>", demangler.Parse("_Z3oneILb1EE"));
  ASSERT_EQ("one<false>", demangler.Parse("_Z3oneILb0EE"));
  ASSERT_EQ("one<false, true>", demangler.Parse("_Z3oneILb0ELb1EE"));

  ASSERT_EQ("one(two<three<four>, false, true>)",
            demangler.Parse("_ZN3oneE3twoI5threeI4fourELb0ELb1EE"));
}

TEST(DemangleTest, non_virtual_thunk) {
  Demangler demangler;

  ASSERT_EQ("non-virtual thunk to one", demangler.Parse("_ZThn0_N3oneE"));
  ASSERT_EQ("non-virtual thunk to two", demangler.Parse("_ZThn0_3two"));
  ASSERT_EQ("non-virtual thunk to three", demangler.Parse("_ZTh0_5three"));
  ASSERT_EQ("non-virtual thunk to four", demangler.Parse("_ZTh_4four"));
  ASSERT_EQ("non-virtual thunk to five", demangler.Parse("_ZTh0123456789_4five"));
  ASSERT_EQ("non-virtual thunk to six", demangler.Parse("_ZThn0123456789_3six"));

  ASSERT_EQ("_ZThn0N3oneE", demangler.Parse("_ZThn0N3oneE"));
  ASSERT_EQ("_ZThn03two", demangler.Parse("_ZThn03two"));
  ASSERT_EQ("_ZTh05three", demangler.Parse("_ZTh05three"));
  ASSERT_EQ("_ZTh4four", demangler.Parse("_ZTh4four"));
  ASSERT_EQ("_ZTh01234567894five", demangler.Parse("_ZTh01234567894five"));
  ASSERT_EQ("_ZThn01234567893six", demangler.Parse("_ZThn01234567893six"));
  ASSERT_EQ("_ZT_N3oneE", demangler.Parse("_ZT_N3oneE"));
  ASSERT_EQ("_ZT0_N3oneE", demangler.Parse("_ZT0_N3oneE"));
  ASSERT_EQ("_ZTH_N3oneE", demangler.Parse("_ZTH_N3oneE"));
}

TEST(DemangleTest, r_value_reference) {
  Demangler demangler;
  ASSERT_EQ(
      "android::SurfaceComposerClient::Transaction::merge(android::SurfaceComposerClient::"
      "Transaction&&)",
      demangler.Parse("_ZN7android21SurfaceComposerClient11Transaction5mergeEOS1_"));
}

TEST(DemangleTest, initial_St) {
  Demangler demangler;
  EXPECT_EQ("std::state", demangler.Parse("_ZSt5state"));
  EXPECT_EQ("std::_In::ward", demangler.Parse("_ZNSt3_In4wardE"));
  EXPECT_EQ("std::__terminate(void (*)())", demangler.Parse("_ZSt11__terminatePFvvE"));
}

TEST(DemangleTest, cfi) {
  Demangler demangler;
  EXPECT_EQ("nfa_sys_ptim_timer_update(tPTIM_CB*)",
            demangler.Parse("_Z25nfa_sys_ptim_timer_updateP8tPTIM_CB"));
  EXPECT_EQ("nfa_sys_ptim_timer_update(tPTIM_CB*) [clone .cfi]",
            demangler.Parse("_Z25nfa_sys_ptim_timer_updateP8tPTIM_CB.cfi"));
}

TEST(DemangleTest, demangle) {
  std::string str;

  str = demangle("_ZN1a1b1cES0_");
  ASSERT_EQ("a::b::c(a::b)", str);

  str = demangle("_");
  ASSERT_EQ("_", str);

  str = demangle("_Z");
  ASSERT_EQ("_Z", str);

  str = demangle("_Za");
  ASSERT_EQ("_Za", str);

  str = demangle("_Zaa");
  ASSERT_EQ("operator&&", str);

  str = demangle("Xa");
  ASSERT_EQ("Xa", str);
}
