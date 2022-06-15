from pathlib import Path

from future.utils import iteritems

import pickle

from pprint import PrettyPrinter

from BinaryParser import BinaryFileParser

# just a small helper function for displaying the available data
def dump_keys(d, lvl=0, reentrant=False):
    """Pretty print a dict showing only keys and size of the
    list entries."""
    if not reentrant:
        print("-----")
    for k, v in iteritems(d):
        if type(v) == list:
            print("{}{}: size {}".format(lvl * ' ', k, len(v)))
        else:
            print("{}{}".format(lvl * ' ', k))
            if type(v) == dict:
                dump_keys(v, lvl+1, reentrant=True)
    if not reentrant:
        print("----")

path_to_file = Path("./all_example_data/basic_example_data/F00000001.bin")

parsed_file = BinaryFileParser(path_to_file)
dump_keys(parsed_file.dict_parsed_data)

print(parsed_file.dict_parsed_data['CHR_parsed'])

