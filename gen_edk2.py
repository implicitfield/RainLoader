#!/usr/bin/python3

import os
import re

if __name__ == '__main__':
    with open('edk2.c', 'w') as output_file:
        output_file.write(
"""/*
 * AUTO GENERATED FILE
 */
 
#include <Uefi.h>
 
""")

        bom = open("edk2core.txt")
        for line in bom:
            output_file.write("/* " + line[:-1].removeprefix("edk2/") + " */\n\n")
            output_file.write(open(line[:-1]).read())
            output_file.write('\n')
