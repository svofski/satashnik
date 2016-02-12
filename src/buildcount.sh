#!/bin/bash
echo $[`cat buildnum` + 1]> buildnum
cat buildnum
