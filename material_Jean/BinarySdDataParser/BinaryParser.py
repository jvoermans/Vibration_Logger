import struct
from pathlib import Path

from raise_assert import ras

import matplotlib.pyplot as plt


class BlockMetadata():
    def __init__(self, metatype, index, start, end):
        self.metatype = metatype
        self.index = index
        self.start = start
        self.end = end


class DataEntry():
    def __init__(self, start, end, data):
        self.start = start
        self.end = end
        self.data = data


class BinaryFileParser():
    ADC_indicator = 65
    CHR_indicator = 67
    n_ADC_entries_per_block = 250

    def __init__(self, path_to_file, n_ADC_channels=5):
        ras(isinstance(path_to_file, Path))

        self.n_ADC_channels = n_ADC_channels
        self.path_to_file = path_to_file

        self.dict_parsed_data = {}
        self.dict_parsed_data["ADC"] = {}
        for crrt_channel in range(self.n_ADC_channels):
            self.dict_parsed_data["ADC"][crrt_channel] = []
        self.dict_parsed_data["CHR"] = []

        self.parse_file()
        self.generate_ADC_timeseries()
        self.assemble_char_data()

    def parse_file(self):
        with open(self.path_to_file, "rb") as fh:
            self.data = fh.read()

        data_length = len(self.data)
        if not (data_length % 512 == 0):
            raise ValueError("not integer number of blocks")

        self.nbr_blocks = int(data_length / 512)

        for crrt_block_ind in range(self.nbr_blocks):
            crrt_block = self.data[512 * crrt_block_ind: 512 * (crrt_block_ind + 1)]
            crrt_metadata, crrt_data = self.parse_data_block(crrt_block)
            crrt_entry = DataEntry(crrt_metadata.start, crrt_metadata.end, crrt_data)

            if crrt_metadata.metatype == "ADC":
                self.dict_parsed_data["ADC"][crrt_metadata.index].append(crrt_entry)

            if crrt_metadata.metatype == "CHR":
                self.dict_parsed_data["CHR"].append(crrt_entry)

    def parse_data_block(self, block):
        # first parse the metadata of the block
        metadata = block[0:12]
        parsed_metadata = struct.unpack('<HHLL', metadata)

        metadata_type = parsed_metadata[0]

        if metadata_type == self.ADC_indicator:
            metadata_type = "ADC"
        elif metadata_type == self.CHR_indicator:
            metadata_type = "CHR"
        else:
            raise ValueError("unknown metadata type")

        metadata = BlockMetadata(metadata_type, parsed_metadata[1],
                                 parsed_metadata[2], parsed_metadata[3])

        # then parse the block content depending on the block type
        data = block[12:512]

        if metadata_type == "ADC":
            format_struct = "<" + 250 * "H"
            data = struct.unpack(format_struct, data)
        elif metadata_type == "CHR":
            pass

        return metadata, data

    def generate_ADC_timeseries(self):
        self.dict_parsed_data["ADC_parsed"] = {}

        for crrt_channel in range(self.n_ADC_channels):
            crrt_list_entries = self.dict_parsed_data["ADC"][crrt_channel]
            list_times = []
            list_readings = []

            for crrt_entry in crrt_list_entries:
                start = crrt_entry.start
                end = crrt_entry.end
                delta_time = float(end - start) / (self.n_ADC_entries_per_block - 1)
                data = crrt_entry.data

                for crrt_value_index in range(self.n_ADC_entries_per_block):
                    crrt_time = start + crrt_value_index * delta_time
                    list_times.append(crrt_time)
                    list_readings.append(data[crrt_value_index])

            self.dict_parsed_data["ADC_parsed"][crrt_channel] = {}
            self.dict_parsed_data["ADC_parsed"][crrt_channel]["times"] = list_times
            self.dict_parsed_data["ADC_parsed"][crrt_channel]["readings"] = list_readings

    def assemble_char_data(self):
        crrt_list_entries = self.dict_parsed_data["CHR"]
        list_chr_data = []

        for crrt_entry in crrt_list_entries:
            chr_data = crrt_entry.data
            list_chr_data.append(chr_data)

        self.dict_parsed_data["CHR_parsed"] = b''.join(list_chr_data)

    def plt_ADC_timeseries(self):
        plt.figure()

        for crrt_channel in range(self.n_ADC_channels):
            crrt_times = self.dict_parsed_data["ADC_parsed"][crrt_channel]["times"]
            crrt_reads = self.dict_parsed_data["ADC_parsed"][crrt_channel]["readings"]
            plt.plot(crrt_times, crrt_reads, label="channel {}".format(crrt_channel))

        plt.xlabel("time [us]")
        plt.ylabel("ADC reading [12 bits]")
        plt.legend(loc="upper right")

        plt.show()


def filename_to_filenumber(filename):
    """turn a standard logger filename into its filenmumber."""
    str_number = filename.name[1:9]
    filenumber = int(str_number)
    return filenumber


def unwrapp_list_micros(list_micros, max_logging_delta=1000000 * 10):
    """given a list_micros of micros readings, possibly that have wrapped, 
    """


class BinaryFolderParser():
    def __init__(self, list_files=None, folder=None, n_ADC_channels=5):
        # be ready to log several adc channels + chars channels
        self.n_ADC_channels = n_ADC_channels

        self.dict_data = {}
        for crrt_channel in range(self.n_ADC_channels):
            self.dict_data["ADC_{}".format(crrt_channel)] = {}
            self.dict_data["ADC_{}".format(crrt_channel)]["micros"] = []
            self.dict_data["ADC_{}".format(crrt_channel)]["readings"] = []

        self.dict_data["CHR"] = []

        # find the list of files to analyze
        if list_files is not None:
            self.list_files = list_files
            ras(folder is None)
        elif folder is not None:
            self.list_files = sorted(list(folder.glob("F*.bin")))
        else:
            raise ValueError("Need either list_files, or folder.")

        # check that the files are in order, without holes
        previous_filenumber = filename_to_filenumber(self.list_files[0])

        for crrt_file in self.list_files[1:]:
            crrt_filenumber = filename_to_filenumber(crrt_file)
            ras(previous_filenumber + 1 == crrt_filenumber,
                "non consecutive filenumbers: {} vs {}".format(previous_filenumber, crrt_filenumber))
            previous_filenumber = crrt_filenumber

        # parse each individual file and put the data together
        for crrt_file in self.list_files:
            binary_file_parser = BinaryFileParser(crrt_file, n_ADC_channels=self.n_ADC_channels)

            self.dict_data["CHR"].extend(str(binary_file_parser.dict_parsed_data["CHR_parsed"])[2:-1])

            for crrt_channel in range(self.n_ADC_channels):
                self.dict_data["ADC_{}".format(crrt_channel)]["micros"].extend(binary_file_parser.dict_parsed_data["ADC_parsed"][crrt_channel]["times"])
                self.dict_data["ADC_{}".format(crrt_channel)]["readings"].extend(binary_file_parser.dict_parsed_data["ADC_parsed"][crrt_channel]["readings"])

        self.dict_data["CHR"] = "".join(self.dict_data["CHR"])

        self.parse_chr_messages()

    def parse_chr_messages(self):
        list_chr_entries = self.dict_data["CHR"].split(";")[2:-2]
        nbr_chr_entries = len(list_chr_entries)

        list_chr_micros = []
        list_chr_messages = []

        crrt_chr_entry_index = 0

        while crrt_chr_entry_index < nbr_chr_entries:
            crrt_entry = list_chr_entries[crrt_chr_entry_index]

            if crrt_entry[0] == 'M' and len(crrt_entry) == 10:
                crrt_chr_entry_index += 1
                next_entry = list_chr_entries[crrt_chr_entry_index]

                list_chr_micros.append(int(crrt_entry[1:]))
                list_chr_messages.append(next_entry)

            crrt_chr_entry_index += 1

        self.dict_data["CHR_micros"] = list_chr_micros
        self.dict_data["CHR_messages"] = list_chr_messages

    def add_utc_timestamps(self):
        pass

    def plt_adc_data(self):
        plt.figure()

        for crrt_channel in range(self.n_ADC_channels):
            crrt_times = self.dict_data["ADC_{}".format(crrt_channel)]["micros"]
            crrt_reads = self.dict_data["ADC_{}".format(crrt_channel)]["readings"]
            plt.plot(crrt_times, crrt_reads, label="channel {}".format(crrt_channel))

        plt.xlabel("time [us]")
        plt.ylabel("ADC reading [12 bits]")
        plt.legend(loc="upper right")

        plt.show()





if __name__ == "__main__":
    import pprint

    pp = pprint.PrettyPrinter(indent=2).pprint

    folder = Path("./example_data/")

    binary_folder_parser = BinaryFolderParser(folder=folder)
    binary_folder_parser.plt_adc_data()
