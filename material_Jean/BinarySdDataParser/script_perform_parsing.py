"""A small example of how to parse bin data and to access it afterhands."""

from pathlib import Path

from future.utils import iteritems

import pickle

from pprint import PrettyPrinter

from BinaryParser import SlidingParser

pp = PrettyPrinter(indent=2).pprint


def dump_keys(d, lvl=0):
    """Pretty print a dict showing only keys and size of the
    list entries."""
    for k, v in iteritems(d):
        if type(v) == list:
            print("{}{}: size {}".format(lvl * ' ', k, len(v)))
        else:
            print("{}{}".format(lvl * ' ', k))
            if type(v) == dict:
                dump_keys(v, lvl+1)

# path to the data to parse
path_to_folder_data = Path("./example_data/")

# this will parse all files
SlidingParser(path_to_folder_data)

# the resulting data are:
# - the metadata:
with open(str(path_to_folder_data.joinpath("sliding_metadata.pkl")), "br") as fh:
    dict_metadata = pickle.load(fh)

pp(dict_metadata)

# - the data corresponding to each file:
with open(str(path_to_folder_data.joinpath("F00000000.pkl")), "br") as fh:
    dict_data_example = pickle.load(fh)

# the keys of any data file should be self explanatory
dump_keys(dict_data_example)

# so to access the data, for example entries 2 and 3 of ADC:
print(dict_data_example["ADC"]["timestamps"][2:4])
print(dict_data_example["ADC"]["channel_0"][2:4])
print(dict_data_example["ADC"]["channel_1"][2:4])

# or accessing some CHR data
print(dict_data_example["CHR"]["timestamps"][2:4])
print(dict_data_example["CHR"]["messages"][2:4])
