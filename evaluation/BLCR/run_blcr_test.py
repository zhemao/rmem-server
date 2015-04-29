#!/usr/bin/env python

# Very simple dummy test / showcase of BLCR running
# Currenly can only run in f1 (only node where BLCR is installed)

import os
import subprocess
import shutil
import re
import time

print "os.getcwd: " + os.getcwd()

print "cr_run ./dummy_test"
p = subprocess.Popen(['cr_run', './dummy_test'], cwd=os.getcwd());
pid = p.pid

time.sleep(2)

print "cr_checkpoint --term " + str(pid)
p = subprocess.Popen(['cr_checkpoint', '--term', str(pid)], cwd=os.getcwd());

for x in range(0,10):
    print "-------------------------------"
print "----------RESTARTING-----------"
for x in range(0,10):
    print "-------------------------------"


time.sleep(1)
print "cr_restart context."+str(pid)
p = subprocess.Popen(['cr_restart', 'context.'+str(pid)], cwd=os.getcwd());

