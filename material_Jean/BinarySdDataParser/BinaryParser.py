import struct
from pathlib import Path

from operator import itemgetter

import numpy as np

import math

import pynmea2

import datetime

from tqdm import tqdm

import pickle

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


class ChannelStats():
    def __init__(self, channel, mean_x, mean_x2, min_val, max_val, count_extrema):
        self.channel = channel
        self.mean_x = mean_x
        self.mean_x2 = mean_x2
        self.min_val = min_val
        self.max_val = max_val
        self.count_extrema = count_extrema
        try:
            self.std = math.sqrt(self.mean_x2 - self.mean_x**2)
        except ValueError as e:
            print("exception {} happened".format(e))
            print("this is because due to rounding errors on the MCU, mean(x^2) slightly less than mean(x)^2")
            print("this can be safely ignored; there was simply very little variation in this signal")
            self.std = 0.0

    def __repr__(self):
        return("< chnl {}: mean {} | mean of sqr {} | min {} | max {} | nbr extremal {} | std {} >".format(
            self.channel,
            self.mean_x,
            self.mean_x2,
            self.min_val,
            self.max_val,
            self.count_extrema,
            self.std
        )
               )


class BinaryFileParser():
    """Parse an individual file, by reading the binary data blocks,
    and generating micros timestamps and corresponding entry lists
    for both ADC and CHR."""
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
            self.dict_parsed_data["ADC_parsed"][crrt_channel]["micros"] = list_times
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
            crrt_times = self.dict_parsed_data["ADC_parsed"][crrt_channel]["micros"]
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


def unwrapp_list_micros(list_micros, wrap_value=2**32-1):
    """Given a list_micros of consecutive micros readings, possibly that have wrapped,
    unwrapp them. Use the fact that, given the present setup, at most 1 wrapping per file,
    and the duration of a file is less than half the wrapping time.
    """
    list_unwrapped_micros = []

    min_micros = min(list_micros)
    max_micros = max(list_micros)
    
    some_wrapping = False
    
    if ((min_micros < 1000000 * 60) and max_micros > (wrap_value - 1000000 * 60)):
        some_wrapping = True
        print("some micro wrapping is taking place")
        
    for crrt_micro in list_micros:
        if ((crrt_micro < wrap_value / 2) and (some_wrapping)):
            crrt_micro += wrap_value
        list_unwrapped_micros.append(crrt_micro)

    return list_unwrapped_micros


class BinaryFolderParser():
    """Parse either the whole content of a folder, or a list of files,
    parsing each file individually and putting the corresponding data
    together. Perform micros to utc datetime regression."""
    # the minimum number of PPS inputs for performing a meaningful fit
    min_nbr_PPS_inputs = 5

    def __init__(self, list_files=None, folder=None, n_ADC_channels=5, show_plots=False):
        self.show_plots = show_plots

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
            ras(folder is None, "need to choose either folder or list of files")
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
                self.dict_data["ADC_{}".format(crrt_channel)]["micros"].extend(
                    binary_file_parser.dict_parsed_data["ADC_parsed"][crrt_channel]["micros"]
                )
                self.dict_data["ADC_{}".format(crrt_channel)]["readings"].extend(
                    binary_file_parser.dict_parsed_data["ADC_parsed"][crrt_channel]["readings"]
                )

        self.dict_data["CHR"] = "".join(self.dict_data["CHR"])

        # parse chars messages: separate the micros timestamps from the messages entries
        self._parse_chr_messages()

        # find the PPS entries and corresponding GPRMC strings
        self._extract_PPS_data()

        # unwrap all micros information
        self._unwrap_all_micros()

        # get the function micros -> timestamp
        self._generate_utc_timestamps_regression(show_fit=self.show_plots)

    def _parse_chr_messages(self):
        list_chr_entries = self.dict_data["CHR"].split(";")[2:-2]
        nbr_chr_entries = len(list_chr_entries)

        list_chr_micros = []
        list_chr_messages = []

        crrt_chr_entry_index = 0

        while crrt_chr_entry_index < nbr_chr_entries - 1:
            crrt_entry = list_chr_entries[crrt_chr_entry_index]

            if crrt_entry[0] == 'M' and len(crrt_entry) == 10:
                crrt_chr_entry_index += 1
                next_entry = list_chr_entries[crrt_chr_entry_index]

                list_chr_micros.append(int(crrt_entry[1:]))
                list_chr_messages.append(next_entry)

            crrt_chr_entry_index += 1

        self.dict_data["CHR_micros"] = list_chr_micros
        self.dict_data["CHR_messages"] = list_chr_messages

    def _extract_PPS_data(self):
        list_PPS_entries = []
        list_PPS_entries_micros = []
        list_PPS_GPRMC_entries = []

        nbr_chr_entries = len(self.dict_data["CHR_messages"])
        crrt_CHR_entry_index = 0

        while crrt_CHR_entry_index < nbr_chr_entries:
            if self.dict_data["CHR_messages"][crrt_CHR_entry_index][0:4] == "PPS:":
                crrt_PPS_entry = self.dict_data["CHR_messages"][crrt_CHR_entry_index]
                while crrt_CHR_entry_index < nbr_chr_entries:
                    if self.dict_data["CHR_messages"][crrt_CHR_entry_index][0:7] == "$GPRMC,":
                        list_PPS_entries.append(crrt_PPS_entry)
                        list_PPS_GPRMC_entries.append(
                            pynmea2.parse(self.dict_data["CHR_messages"][crrt_CHR_entry_index].split('\\')[0])
                        )
                        list_PPS_entries_micros.append(int(crrt_PPS_entry[4:]))
                        break
                    crrt_CHR_entry_index += 1

            crrt_CHR_entry_index += 1

        self.dict_data["PPS_entries"] = list_PPS_entries
        self.dict_data["PPS_entries_micros"] = list_PPS_entries_micros
        self.dict_data["PPS_GPRMC_entries"] = list_PPS_GPRMC_entries

    def _unwrap_all_micros(self):
        self.dict_data["CHR_micros_unwrapped"] = unwrapp_list_micros(self.dict_data["CHR_micros"])
        self.dict_data["ADC_micros_unwrapped"] = unwrapp_list_micros(self.dict_data["ADC_0"]["micros"])
        self.dict_data["PPS_entries_micros_unwrapped"] = unwrapp_list_micros(self.dict_data["PPS_entries_micros"])

    def _generate_utc_timestamps_regression(self, show_fit=False):
        # regression from timestamp of PPS_GPRMC_entries to PPS_entries_micros_unwrapped
        # this gives a function micros -> timestamp
        # can then perform timestamp -> datetime

        # assemble date and time
        list_PPS_datetimes = [
            datetime.datetime.combine(
                crrt_entry.datestamp, crrt_entry.timestamp, tzinfo=datetime.timezone.utc
            ) for crrt_entry in self.dict_data["PPS_GPRMC_entries"]
        ]

        # enough PPS inputs to perform the fitting
        if len(list_PPS_datetimes) > self.min_nbr_PPS_inputs:
            # create list of valid fixes
            list_valid_fixes = [(crrt_entry.status == 'A') for crrt_entry in self.dict_data["PPS_GPRMC_entries"]]

            # keep only valid fixes for doing the fit
            valid_PPS_entries = [ind for ind, valid_fix in enumerate(list_valid_fixes) if valid_fix]
            valid_PPS_getter = itemgetter(*valid_PPS_entries)

            # perform the fitting
            unrolled_arduino_PPS_micros = list(valid_PPS_getter(self.dict_data["PPS_entries_micros_unwrapped"]))
            PPS_timestamps = valid_PPS_getter([crrt_datetime.timestamp() for crrt_datetime in list_PPS_datetimes])

            degree = 1
            coeffs = np.polyfit(unrolled_arduino_PPS_micros, PPS_timestamps, degree)
            self.polyfit_unrolled_micros_to_timestamp = np.poly1d(coeffs)

            def fn_unrolled_arduino_micros_to_datetime(unrolled_arduino_micro):
                ras(isinstance(unrolled_arduino_micro, list))
                timestamp = self.polyfit_unrolled_micros_to_timestamp(unrolled_arduino_micro)
                datetimes = [
                    datetime.datetime.utcfromtimestamp(crrt_timestamp).replace(tzinfo=datetime.timezone.utc)
                    for crrt_timestamp in timestamp
                ]
                return(datetimes)

            self.fn_unrolled_arduino_micros_to_datetime = fn_unrolled_arduino_micros_to_datetime

            self.valid_PPS = True

            if show_fit:
                datetime_from_unrolled_PPS_micros = self.fn_unrolled_arduino_micros_to_datetime(unrolled_arduino_PPS_micros)

                list_deviation_PPS_micros_vs_datetime = []

                for ind in range(len(datetime_from_unrolled_PPS_micros)):
                    crrt_from_micros = datetime_from_unrolled_PPS_micros[ind]
                    crrt_from_GPRMC = list_PPS_datetimes[ind]

                    list_deviation_PPS_micros_vs_datetime.append((crrt_from_GPRMC - crrt_from_micros).total_seconds())

                plt.figure()
                plt.plot(unrolled_arduino_PPS_micros, list_deviation_PPS_micros_vs_datetime)
                plt.xlabel("arduino unrolled micros timestamp")
                plt.ylabel("deviation GPRMC time vs. arduino micros converted to datetime [seconds]")
                plt.show()

        # not enough PPS input to perform the fiting: do none
        else:
            print("Not enough PPS inputs to perform a time fitting")
            print("This may indicate that no valid GPS fix is obtained")
            print("We now use identity as PPS fitting, which will mis-offset")
            print("all time measurements to the UNIX period start ie")
            print("01-01-1970")

            def identity(unrolled_arduino_micro):
                ras(isinstance(unrolled_arduino_micro, list))
                datetimes = [
                    datetime.datetime.utcfromtimestamp(crrt_timestamp * 0.000001).replace(tzinfo=datetime.timezone.utc)
                    for crrt_timestamp in unrolled_arduino_micro
                ]
                return(datetimes)

            self.fn_unrolled_arduino_micros_to_datetime = identity

            self.valid_PPS = False

    def plt_adc_data(self):
        """Plot the ADC data."""
        plt.figure()

        for crrt_channel in range(self.n_ADC_channels):
            crrt_times = self.fn_unrolled_arduino_micros_to_datetime(self.dict_data["ADC_{}".format(crrt_channel)]["micros"])
            crrt_reads = self.dict_data["ADC_{}".format(crrt_channel)]["readings"]
            plt.plot(crrt_times, crrt_reads, label="channel {}".format(crrt_channel))

        plt.ylabel("ADC reading [12 bits]")
        plt.legend(loc="upper right")

        if not self.valid_PPS:
            plt.xlabel("no PPS as no GPS fix; dates wrongly offser")

        plt.show()

    def get_ADC_data(self):
        """Get the ADC data. returns a tuple (timestamps, ADC_data).
        The timestamps is a list of UTC datetimes. The ADC_data is a
        list of ADC channels data, where each ADC channels data is a list
        of measurements. Ie., ADC_data[0][:] are the data for the channel
        0, where the measurements correpond to the times in timestamps."""
        list_ADC_data = []

        for crrt_channel in range(self.n_ADC_channels):
            list_ADC_data.append(self.dict_data["ADC_{}".format(crrt_channel)]["readings"])

        return (self.fn_unrolled_arduino_micros_to_datetime(self.dict_data["ADC_micros_unwrapped"]),
                list_ADC_data)

    def get_CHR_messages(self):
        """Get all available CHR messages. Returns a tuple (timestamps, CHR_messages),
        where timestamps is a list of UTC datetimes, and CHR_messages is a list of
        messages, which were received at the timestamps times."""
        return (self.fn_unrolled_arduino_micros_to_datetime(self.dict_data["CHR_micros_unwrapped"]),
                self.dict_data["CHR_messages"])


class SlidingParser():
    """Perform a 'sliding parsing' of all the files in a folder. This allows to
    both save memory as only a couple of files are loaded at the same time,
    but take care that all messages are fully reconstructed. Dump the parsed data
    in 1 pkl file per binary file."""
    def __init__(self, folder):
        self.folder = folder
        ras(isinstance(self.folder, Path))
        self.list_files = sorted(list(self.folder.glob("F*.bin")))

        self.dict_metadata = {}
        self.dict_metadata["ADC"] = {}
        self.dict_metadata["CHR"] = {}

        # when parsing, always dump until the end of the ADC data, and until
        # the end of the previous last CHR data

        time_start_CHR = datetime.datetime(1970, 1, 1, 0, 0, 0, tzinfo=datetime.timezone.utc)
        time_start_ADC = datetime.datetime(1970, 1, 1, 0, 0, 0, tzinfo=datetime.timezone.utc)

        # parse the first file alone
        time_start_CHR, time_start_ADC = self._process_entries([self.list_files[0]], time_start_CHR, time_start_ADC)

        # from now on, parse all files in pairs
        for crrt_ind_start in tqdm(range(len(self.list_files) - 1)):
            crrt_pair_files = [self.list_files[crrt_ind_start], self.list_files[crrt_ind_start + 1]]
            time_start_CHR, time_start_ADC = self._process_entries(crrt_pair_files, time_start_CHR, time_start_ADC)

        path_dump_metadata = folder.joinpath("sliding_metadata.pkl")

        with open(str(path_dump_metadata), "wb") as fh:
            pickle.dump(self.dict_metadata, fh)

    def _process_entries(self, list_sliding_files, time_start_CHR, time_start_ADC):
        """list_sliding_files: the files to look at
        time_start_CHR: the (excluded, start after) time where to start dumping CHR
        time_start_ADC: the (excluded, start after) time where to start dumping ADC.
        Return the time of the last CHR and ADC dump."""

        # under which path to dump
        path_dump = list_sliding_files[-1]
        filename = path_dump.stem + ".pkl"
        folder = path_dump.parent
        dump_path = folder.joinpath(filename)

        # the data object to dump
        dict_data = {}
        dict_data["ADC"] = {}
        dict_data["CHR"] = {}

        # parse
        binary_folder_parser = BinaryFolderParser(list_files=list_sliding_files)

        # dump only the necessary part, and update the end of dumping times
        timestamps_ADC, data_ADC = binary_folder_parser.get_ADC_data()
        idx_timestamp_start = np.searchsorted(np.array(timestamps_ADC), time_start_ADC, side="right")

        dict_data["ADC"]["timestamps"] = timestamps_ADC[idx_timestamp_start:]

        for crrt_channel in range(len(data_ADC)):
            dict_data["ADC"]["channel_{}".format(crrt_channel)] = data_ADC[crrt_channel][idx_timestamp_start:]

        time_last_dumped_ADC = timestamps_ADC[-1]

        self.dict_metadata["ADC"]["time_limits_{}".format(filename)] = \
            [timestamps_ADC[idx_timestamp_start], timestamps_ADC[-1]]

        timestamps_CHR, data_CHR = binary_folder_parser.get_CHR_messages()
        idx_timestamp_start = np.searchsorted(np.array(timestamps_CHR), time_start_CHR, side="right")

        dict_data["CHR"]["timestamps"] = timestamps_CHR[idx_timestamp_start:-1]
        dict_data["CHR"]["messages"] = data_CHR[idx_timestamp_start:-1]

        time_last_dumped_CHR = timestamps_CHR[-2]

        self.dict_metadata["CHR"]["time_limits_{}".format(filename)] = \
            [timestamps_CHR[idx_timestamp_start], timestamps_CHR[-2]]

        with open(str(dump_path), "wb") as fh:
            pickle.dump(dict_data, fh)

        return (time_last_dumped_CHR, time_last_dumped_ADC)


def GPRMC_extractor(dict_data):
    list_CHR_messages = dict_data["CHR"]["messages"]
    list_parsed_GPRMC = []

    for crrt_msg in list_CHR_messages:
        if crrt_msg[0:6] == "$GPRMC":
            crrt_parsed_gprmc = pynmea2.parse(crrt_msg.split('\\')[0])
            list_parsed_GPRMC.append(crrt_parsed_gprmc)

    return list_parsed_GPRMC


def temperatures_extractor(dict_data):
    list_CHR_messages = dict_data["CHR"]["messages"]
    list_CHR_timestamps = dict_data["CHR"]["timestamps"]

    list_temperatures_timestamps = []
    list_temperatures_readings = []

    for (crrt_timestamp, crrt_message) in zip(list_CHR_timestamps, list_CHR_messages):
        if crrt_message[0:4] == "TMP,":
            crrt_list_temperatures = []
            crrt_temperatures_readings = (crrt_message[4:]).split(",")

            for crrt_reading in crrt_temperatures_readings:
                if crrt_reading != "":
                    crrt_list_temperatures.append(float(crrt_reading))

            list_temperatures_timestamps.append(crrt_timestamp)
            list_temperatures_readings.append(crrt_list_temperatures)

    return (list_temperatures_timestamps, list_temperatures_readings)

def channel_stats_extractor(dict_data):
    list_CHR_messages = dict_data["CHR"]["messages"]
    list_CHR_timestamps = dict_data["CHR"]["timestamps"]

    list_stats_timestamps = []
    list_stats_readings = []

    for (crrt_timestamp, crrt_message) in zip(list_CHR_timestamps, list_CHR_messages):
        if crrt_message[0:4] == "STAT":
            crrt_chnl = int(crrt_message[4:6])

            list_field_chars = crrt_message[7:].split(",")

            crrt_stat = ChannelStats(
                channel = crrt_chnl,
                mean_x = float(list_field_chars[0]),
                mean_x2 = float(list_field_chars[1]),
                min_val = float(list_field_chars[3]),
                max_val = float(list_field_chars[2]),
                count_extrema = float(list_field_chars[4])
            )

            list_stats_timestamps.append(crrt_timestamp)
            list_stats_readings.append(crrt_stat)

    return (list_stats_timestamps, list_stats_readings)
