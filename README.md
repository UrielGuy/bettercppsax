# bettercppsax

This repository hold bettercppsax, a headers only C++20 library (other than its dependencies) to help build
efficient, easy and clean parsers for JSON and YAML files on top of industry standard SAX parsers. 

## What is this all about? 

Most JSON and YAML parsing libraries use the DOM operation mode. DOM mean loading all of the data in a file
into memory, in a relatively large data model. This both means holding all of the data in memory and at least 
two processing interations - one to load the JSON/YAML data, and one to go over the data structures. 
Alternatively, one can use the SAX parsing model.
