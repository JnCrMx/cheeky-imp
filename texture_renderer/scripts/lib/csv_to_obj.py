#!/usr/bin/env python

import csv
import sys
import numpy as np

matrix = np.array([
    # put your projection matrix here
])
matrix = np.transpose(matrix)
matrix = np.linalg.inv(matrix)

vertices=[]
with open(sys.argv[1], newline='') as csvfile:
    reader = csv.reader(csvfile, delimiter=',', quotechar='"')
    next(reader)
    i = 1
    for row in reader:
        x = float(row[2])
        y = float(row[3])
        z = float(row[4])
        w = float(row[5])
        vec = np.array([x, y, z, w])
        vec = matrix @ vec
        print(f"v {vec[0]} {vec[1]} {vec[2]}")
        if i%3 == 0:
            print(f"f {i-2} {i-1} {i-0}")
        i+=1
