from string import Template
import argparse
import os

parser = argparse.ArgumentParser(description='Generate the files based on Rust User Library')

parser.add_argument('-d', '--dir', action='store', type=str)
parser.add_argument('-t', '--task_amount', action='store', type=int, required=True)
parser.add_argument('-n', '--task_names', action='store', nargs='+', required=True)
parser.add_argument('-z', '--zbus_observer_amount', action='store', type=int)

args = parser.parse_args()

directory = '../../../src/generated'
if args.dir:
    directory = os.path.abspath(args.dir)

if not os.path.exists(directory):
    os.makedirs(directory)

if directory[-1] != '/' and directory[-1] != '\\':
    directory += '/'

rart_defines_file = directory + 'rart-defines.h'
with open(rart_defines_file, 'w') as file:
    t = Template("""/**
 * @file rart-defines.h
 * @brief File generated with user definitions and function signatures for RART
 * @version 0.1
 */

#ifndef RART_DEFINES_H
#define RART_DEFINES_H

#define NUM_OF_TASKS $task_num
$task_list
#endif  /* RART_DEFINES_H */""")
    task_list = ''
    for name in args.task_names:
        task_list += f'\nvoid {name}(void);\n'

    content = t.substitute(task_num=args.task_amount, task_list=task_list)
    file.write(content)

if args.zbus_observer_amount:
    zbus_backend_defines_file = directory + 'zbus-backend-defines.h'
    with open(zbus_backend_defines_file, 'w') as file:
        t = Template("""/**
 * @file zbus-backend-defines.h
 * @brief File generated with user definitions for ZBUS Backend
 * @version 0.1
 */

#ifndef ZBUS_BACKEND_DEFINES_H
#define ZBUS_BACKEND_DEFINES_H

#define NUM_OF_OBSERVERS $observer_num

#endif  /* ZBUS_BACKEND_DEFINES_H */""")
        content = t.substitute(observer_num=args.zbus_observer_amount)
        file.write(content)
