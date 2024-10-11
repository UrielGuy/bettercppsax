cmake_minimum_required(VERSION 3.13)

project(SIMDJSON VERSION 0.1.0 LANGUAGES CXX)

add_library(simdjson STATIC simdjson.h simdjson.cpp )
