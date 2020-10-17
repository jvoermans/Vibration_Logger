import struct

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
    n_ADC_channels = 5
    n_ADC_entries_per_block = 250

    def __init__(self, path_to_file):
        self.path_to_file = path_to_file

        self.dict_parsed_data = {}
        self.dict_parsed_data["ADC"] = {}
        for crrt_channel in range(self.n_ADC_channels):
            self.dict_parsed_data["ADC"][crrt_channel] = []
        self.dict_parsed_data["CHR"] = []

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





if __name__ == "__main__":
    import pprint

    pp = pprint.PrettyPrinter(indent=2).pprint

    path_to_file = "./example_data/F00000000.bin"
    binary_file_parser = BinaryFileParser(path_to_file)
    binary_file_parser.parse_file()
    binary_file_parser.generate_ADC_timeseries()

    binary_file_parser.plt_ADC_timeseries()
