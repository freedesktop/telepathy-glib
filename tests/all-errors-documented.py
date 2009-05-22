#!/usr/bin/python
# Check if all the errors have been added to
# docs/reference/telepathy-glib-sections.txt

import os
import sys

import xml.dom.minidom

from libglibcodegen import NS_TP, camelcase_to_upper

def check_all_errors_documented(abs_top_builddir):
    error_path = os.path.join(abs_top_builddir, 'spec', 'errors.xml')
    sections_path = os.path.join(abs_top_builddir, 'docs', 'reference',
        'telepathy-glib-sections.txt')
    sections = open(sections_path).readlines()

    dom = xml.dom.minidom.parse(error_path)

    errors = dom.getElementsByTagNameNS(NS_TP, 'errors')[0]
    for error in errors.getElementsByTagNameNS(NS_TP, 'error'):
        nick = error.getAttribute('name').replace(' ', '')
        name = 'TP_ERROR_STR_' + camelcase_to_upper(nick.replace('.', ''))

        if '%s\n' % name not in sections:
            print "'%s' is missing in %s" % (name, sections_path)
            sys.exit(1)

if __name__ == '__main__':
    check_all_errors_documented(os.environ['abs_top_builddir'])
