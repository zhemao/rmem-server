#!/usr/bin/env python

# Very simple dummy test / showcase of BLCR running
# Currenly can only run in f1 (only node where BLCR is installed)

import os
import subprocess
import shutil
import re
import time
import sys

coordinator_pid = 0
server_pid      = 0
zook_dir1       = '/nscratch/joao/zookeeper_bits/data/zookeeper1'
zook_dir2       = '/nscratch/joao/zookeeper_bits/data/zookeeper2'
zook_dir3       = '/nscratch/joao/zookeeper_bits/data/zookeeper3'

zookeeper_dir = '/nscratch/joao/repos/bits/zookeeper'
master_dir = '/nscratch/joao/ramcloud/obj.master'

if sys.argv[1] == "stop":
    srun_cmd = ['pkill','srun']
    p = subprocess.Popen(srun_cmd)

    p = subprocess.Popen(['python', 'zookeeper.py', 'stop'], cwd=zookeeper_dir)

    p = subprocess.Popen(['rm', 'version-2', '-rf'], cwd=zook_dir1)
    p = subprocess.Popen(['rm', 'version-2', '-rf'], cwd=zook_dir2)
    p = subprocess.Popen(['rm', 'version-2', '-rf'], cwd=zook_dir3)

elif sys.argv[1] == "start":
    p = subprocess.Popen(['python', 'zookeeper.py', 'start'], cwd=zookeeper_dir)
    zook_pid = p.pid

    time.sleep(2)

    srun_cmd = ['srun','-N1', '--nodelist=f2']
    srun_cmd += ['./coordinator', '-C', 'infrc:host=f2,port=11100','-x','zk:f16:2181']
    p = subprocess.Popen(srun_cmd, cwd=master_dir)
    coordinator_pid = p.pid

    time.sleep(2)

    srun_cmd = ['srun','-N1', '--nodelist=f3']
    srun_cmd += ['./server', '-L', 'infrc:host=f3,port=11101','-x','zk:f16:2181']
    srun_cmd += ['-C', 'infrc:host=f2,port=11100']
    p = subprocess.Popen(srun_cmd, cwd=master_dir)
    server_pid = p.pid

