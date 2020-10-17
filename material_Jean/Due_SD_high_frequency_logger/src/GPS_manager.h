// start
// set serial port rx
// set PPS pin: any pin on the due
// all messages are cstrings
// function message_available, including PPS
// get pointer to available message
// TODO: make sure larger buffer on GPS serial receive

// available_message: check PPS, check serial, generate message, return bool
// get_message: pointer to the cstring message, anc reset
// have an internal "ready to send message" cstring buffer, length 256 bytes

// make sure buffers are big enough
// if necessary, modify in the core
static_assert(SERIAL_BUFFER_SIZE >= 512);