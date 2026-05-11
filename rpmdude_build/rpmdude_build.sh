#!/bin/bash
set -e
TOPDIR=$(pwd)
rpmbuild --define "_topdir $TOPDIR" -ba SPECS/justcolor.spec
