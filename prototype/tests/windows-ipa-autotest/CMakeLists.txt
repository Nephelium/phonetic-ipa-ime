cmake_minimum_required(VERSION 3.20)
project(PhoneticIpaAutoTest LANGUAGES CXX)
# Console tool; see main.cpp for its safety boundary (own window, own process, focus-checked keys).
add_executable(ipa-autotest main.cpp)
target_compile_features(ipa-autotest PRIVATE cxx_std_17)
target_compile_definitions(ipa-autotest PRIVATE UNICODE _UNICODE)
target_compile_options(ipa-autotest PRIVATE /utf-8)
target_link_libraries(ipa-autotest PRIVATE ole32 uuid user32 gdi32)
