ADD_SUBDIRECTORY (utils)
ADD_SUBDIRECTORY (lizard)        # common lizard code for both standalone and dynamic module versions
ADD_SUBDIRECTORY (lizard-static) # standalone version - one binary file from lizard core and plugin code
ADD_SUBDIRECTORY (test-static)   # plugin example for standlone version
ADD_SUBDIRECTORY (lizard-module) # dynamic module version - binary server file loads plugin at runtime from shared library
ADD_SUBDIRECTORY (test-module)   # plugin example for module version
