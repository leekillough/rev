
#define DATA_SIZE 1000
#define PADDING   32
static data_t input0                           = 8.00;
static data_t input1_data[DATA_SIZE + PADDING] = {
  15.00, 10.00, 3.00,  14.00, 6.00,  2.00,  18.00, 11.00, 15.00, 11.00, 0.00,  17.00, 16.00, 7.00,  13.00, 18.00, 2.00,  2.00,
  5.00,  8.00,  5.00,  12.00, 14.00, 6.00,  12.00, 16.00, 7.00,  9.00,  17.00, 10.00, 10.00, 3.00,  5.00,  14.00, 11.00, 9.00,
  12.00, 3.00,  1.00,  5.00,  4.00,  6.00,  17.00, 17.00, 4.00,  17.00, 15.00, 17.00, 18.00, 3.00,  18.00, 10.00, 6.00,  12.00,
  12.00, 3.00,  2.00,  16.00, 1.00,  5.00,  6.00,  2.00,  17.00, 16.00, 5.00,  10.00, 18.00, 14.00, 4.00,  9.00,  9.00,  7.00,
  4.00,  8.00,  13.00, 12.00, 6.00,  4.00,  17.00, 5.00,  2.00,  11.00, 11.00, 7.00,  6.00,  8.00,  5.00,  6.00,  6.00,  11.00,
  1.00,  18.00, 7.00,  6.00,  9.00,  10.00, 17.00, 14.00, 4.00,  14.00, 11.00, 2.00,  0.00,  2.00,  17.00, 17.00, 15.00, 10.00,
  8.00,  12.00, 18.00, 5.00,  0.00,  0.00,  1.00,  1.00,  8.00,  11.00, 11.00, 7.00,  5.00,  15.00, 18.00, 15.00, 6.00,  8.00,
  9.00,  18.00, 6.00,  15.00, 16.00, 10.00, 18.00, 13.00, 6.00,  10.00, 16.00, 0.00,  10.00, 12.00, 8.00,  8.00,  3.00,  0.00,
  1.00,  4.00,  0.00,  8.00,  15.00, 16.00, 9.00,  7.00,  7.00,  10.00, 9.00,  16.00, 12.00, 5.00,  5.00,  9.00,  5.00,  17.00,
  4.00,  13.00, 13.00, 4.00,  14.00, 10.00, 5.00,  10.00, 10.00, 6.00,  18.00, 5.00,  15.00, 5.00,  15.00, 13.00, 18.00, 6.00,
  13.00, 5.00,  18.00, 12.00, 14.00, 1.00,  7.00,  2.00,  1.00,  7.00,  12.00, 15.00, 15.00, 6.00,  2.00,  1.00,  2.00,  18.00,
  6.00,  10.00, 14.00, 9.00,  15.00, 11.00, 0.00,  3.00,  1.00,  2.00,  2.00,  14.00, 7.00,  18.00, 0.00,  2.00,  2.00,  16.00,
  8.00,  4.00,  5.00,  7.00,  11.00, 7.00,  10.00, 14.00, 10.00, 3.00,  0.00,  13.00, 9.00,  14.00, 0.00,  14.00, 1.00,  3.00,
  16.00, 2.00,  14.00, 6.00,  17.00, 17.00, 6.00,  9.00,  8.00,  9.00,  12.00, 5.00,  14.00, 2.00,  16.00, 17.00, 5.00,  4.00,
  0.00,  14.00, 10.00, 6.00,  15.00, 15.00, 14.00, 5.00,  1.00,  8.00,  8.00,  13.00, 8.00,  0.00,  4.00,  7.00,  9.00,  13.00,
  16.00, 9.00,  14.00, 9.00,  13.00, 0.00,  7.00,  16.00, 17.00, 18.00, 10.00, 13.00, 8.00,  4.00,  9.00,  13.00, 0.00,  6.00,
  4.00,  6.00,  4.00,  10.00, 14.00, 14.00, 9.00,  15.00, 15.00, 3.00,  3.00,  12.00, 0.00,  3.00,  2.00,  16.00, 1.00,  7.00,
  2.00,  16.00, 2.00,  2.00,  0.00,  14.00, 3.00,  3.00,  10.00, 4.00,  10.00, 3.00,  4.00,  13.00, 14.00, 13.00, 0.00,  1.00,
  15.00, 16.00, 9.00,  7.00,  9.00,  0.00,  11.00, 1.00,  1.00,  15.00, 17.00, 12.00, 16.00, 15.00, 4.00,  8.00,  2.00,  10.00,
  10.00, 0.00,  18.00, 17.00, 7.00,  7.00,  2.00,  10.00, 17.00, 9.00,  7.00,  5.00,  3.00,  8.00,  11.00, 6.00,  9.00,  13.00,
  0.00,  3.00,  5.00,  2.00,  5.00,  7.00,  4.00,  9.00,  2.00,  13.00, 17.00, 14.00, 12.00, 1.00,  3.00,  7.00,  17.00, 0.00,
  14.00, 16.00, 2.00,  2.00,  1.00,  2.00,  15.00, 16.00, 8.00,  2.00,  4.00,  15.00, 15.00, 10.00, 6.00,  11.00, 9.00,  15.00,
  17.00, 3.00,  8.00,  15.00, 3.00,  10.00, 15.00, 8.00,  14.00, 16.00, 15.00, 15.00, 14.00, 1.00,  7.00,  4.00,  18.00, 2.00,
  13.00, 11.00, 15.00, 7.00,  10.00, 13.00, 10.00, 7.00,  14.00, 18.00, 4.00,  18.00, 4.00,  3.00,  9.00,  1.00,  13.00, 15.00,
  2.00,  5.00,  15.00, 12.00, 10.00, 2.00,  0.00,  10.00, 15.00, 0.00,  11.00, 14.00, 11.00, 14.00, 9.00,  1.00,  18.00, 14.00,
  18.00, 15.00, 5.00,  1.00,  15.00, 18.00, 14.00, 3.00,  15.00, 11.00, 2.00,  15.00, 0.00,  13.00, 1.00,  4.00,  14.00, 14.00,
  5.00,  2.00,  13.00, 17.00, 8.00,  7.00,  6.00,  5.00,  10.00, 14.00, 14.00, 17.00, 0.00,  0.00,  17.00, 18.00, 15.00, 10.00,
  16.00, 18.00, 5.00,  9.00,  10.00, 18.00, 7.00,  11.00, 5.00,  4.00,  16.00, 2.00,  8.00,  13.00, 1.00,  12.00, 3.00,  4.00,
  6.00,  15.00, 12.00, 0.00,  6.00,  18.00, 12.00, 14.00, 18.00, 3.00,  2.00,  3.00,  5.00,  3.00,  14.00, 18.00, 12.00, 10.00,
  11.00, 8.00,  4.00,  10.00, 10.00, 9.00,  18.00, 14.00, 3.00,  7.00,  17.00, 12.00, 0.00,  10.00, 9.00,  17.00, 3.00,  0.00,
  4.00,  6.00,  16.00, 14.00, 12.00, 13.00, 13.00, 18.00, 7.00,  0.00,  1.00,  9.00,  7.00,  12.00, 6.00,  18.00, 8.00,  9.00,
  13.00, 13.00, 17.00, 10.00, 16.00, 1.00,  10.00, 17.00, 16.00, 2.00,  18.00, 4.00,  2.00,  6.00,  1.00,  1.00,  1.00,  8.00,
  14.00, 6.00,  6.00,  13.00, 14.00, 13.00, 6.00,  5.00,  10.00, 11.00, 11.00, 16.00, 1.00,  5.00,  9.00,  13.00, 8.00,  10.00,
  2.00,  12.00, 15.00, 5.00,  14.00, 3.00,  7.00,  9.00,  18.00, 2.00,  11.00, 16.00, 4.00,  5.00,  10.00, 17.00, 10.00, 3.00,
  4.00,  14.00, 18.00, 13.00, 6.00,  8.00,  11.00, 14.00, 3.00,  5.00,  6.00,  6.00,  5.00,  13.00, 0.00,  9.00,  9.00,  1.00,
  7.00,  5.00,  5.00,  1.00,  6.00,  18.00, 11.00, 17.00, 7.00,  1.00,  10.00, 5.00,  12.00, 6.00,  16.00, 16.00, 5.00,  1.00,
  10.00, 10.00, 15.00, 7.00,  18.00, 8.00,  17.00, 3.00,  5.00,  3.00,  14.00, 0.00,  16.00, 12.00, 0.00,  14.00, 17.00, 16.00,
  2.00,  18.00, 13.00, 10.00, 13.00, 4.00,  14.00, 2.00,  3.00,  4.00,  8.00,  17.00, 0.00,  6.00,  11.00, 5.00,  3.00,  3.00,
  2.00,  15.00, 13.00, 10.00, 4.00,  1.00,  11.00, 6.00,  17.00, 1.00,  0.00,  18.00, 3.00,  3.00,  11.00, 7.00,  7.00,  11.00,
  14.00, 7.00,  16.00, 11.00, 10.00, 8.00,  6.00,  11.00, 5.00,  17.00, 10.00, 7.00,  8.00,  14.00, 2.00,  9.00,  17.00, 15.00,
  13.00, 10.00, 6.00,  0.00,  15.00, 11.00, 10.00, 11.00, 18.00, 2.00,  5.00,  17.00, 18.00, 11.00, 15.00, 3.00,  17.00, 9.00,
  17.00, 8.00,  6.00,  2.00,  4.00,  2.00,  11.00, 15.00, 2.00,  18.00, 3.00,  9.00,  7.00,  15.00, 9.00,  14.00, 10.00, 9.00,
  6.00,  13.00, 8.00,  15.00, 14.00, 0.00,  11.00, 5.00,  2.00,  12.00, 14.00, 10.00, 16.00, 9.00,  7.00,  9.00,  17.00, 4.00,
  4.00,  7.00,  8.00,  4.00,  4.00,  9.00,  7.00,  3.00,  5.00,  11.00, 11.00, 10.00, 13.00, 3.00,  14.00, 15.00, 8.00,  1.00,
  1.00,  3.00,  0.00,  16.00, 9.00,  6.00,  1.00,  0.00,  2.00,  0.00,  6.00,  13.00, 12.00, 5.00,  18.00, 1.00,  11.00, 17.00,
  11.00, 16.00, 14.00, 14.00, 9.00,  11.00, 9.00,  17.00, 15.00, 5.00,  18.00, 2.00,  11.00, 10.00, 16.00, 18.00, 5.00,  11.00,
  12.00, 11.00, 18.00, 7.00,  6.00,  8.00,  3.00,  4.00,  3.00,  16.00, 4.00,  6.00,  2.00,  15.00, 6.00,  7.00,  16.00, 0.00,
  7.00,  11.00, 10.00, 3.00,  0.00,  14.00, 16.00, 15.00, 15.00, 12.00, 7.00,  1.00,  4.00,  8.00,  4.00,  12.00, 0.00,  7.00,
  8.00,  1.00,  1.00,  14.00, 15.00, 9.00,  8.00,  6.00,  6.00,  4.00,  7.00,  8.00,  13.00, 10.00, 5.00,  8.00,  11.00, 2.00,
  16.00, 7.00,  17.00, 5.00,  2.00,  17.00, 0.00,  18.00, 6.00,  7.00,  4.00,  4.00,  12.00, 0.00,  18.00, 8.00,  4.00,  7.00,
  0.00,  11.00, 1.00,  11.00, 17.00, 18.00, 15.00, 8.00,  11.00, 15.00, 9.00,  12.00, 1.00,  5.00,  6.00,  1.00,  18.00, 14.00,
  7.00,  16.00, 16.00, 10.00, 3.00,  13.00, 0.00,  12.00, 9.00,  18.00, 14.00, 15.00, 4.00,  11.00, 15.00, 15.00, 8.00,  16.00,
  11.00, 13.00, 12.00, 1.00,  13.00, 14.00, 2.00,  11.00, 0.00,  17.00, 11.00, 12.00, 6.00,  4.00,  4.00,  11.00, 13.00, 10.00,
  2.00,  10.00, 14.00, 0.00,  6.00,  18.00, 10.00, 7.00,  14.00, 12.00, 9.00,  4.00,  16.00, 17.00, 8.00,  14.00, 9.00,  0.00,
  4.00,  15.00, 13.00, 8.00,  13.00, 13.00, 8.00,  15.00, 6.00,  11.00, 4.00,  2.00,  6.00,  5.00,  14.00, 5.00,  17.00, 12.00,
  11.00, 17.00, 4.00,  13.00, 7.00,  16.00, 12.00, 7.00,  18.00, 12.00, 0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,
  0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,
  0.0,   0.0,   0.0,   0.0,   0.0,   0.0
};

static data_t input2_data[DATA_SIZE + PADDING] = {
  6.00,  0.00,  18.00, 6.00,  10.00, 1.00,  2.00,  4.00,  2.00,  4.00,  10.00, 6.00,  11.00, 17.00, 4.00,  0.00,  16.00, 14.00,
  12.00, 9.00,  8.00,  13.00, 15.00, 18.00, 2.00,  13.00, 10.00, 5.00,  4.00,  12.00, 9.00,  1.00,  13.00, 12.00, 7.00,  10.00,
  17.00, 11.00, 10.00, 18.00, 15.00, 11.00, 12.00, 7.00,  9.00,  6.00,  5.00,  11.00, 7.00,  10.00, 12.00, 18.00, 18.00, 15.00,
  1.00,  3.00,  18.00, 11.00, 16.00, 13.00, 18.00, 4.00,  13.00, 8.00,  7.00,  10.00, 13.00, 0.00,  9.00,  1.00,  16.00, 13.00,
  7.00,  5.00,  5.00,  11.00, 1.00,  6.00,  10.00, 12.00, 3.00,  10.00, 5.00,  15.00, 15.00, 13.00, 14.00, 14.00, 1.00,  18.00,
  5.00,  16.00, 14.00, 10.00, 4.00,  8.00,  4.00,  6.00,  6.00,  13.00, 14.00, 8.00,  5.00,  14.00, 10.00, 9.00,  10.00, 17.00,
  15.00, 6.00,  16.00, 12.00, 9.00,  10.00, 16.00, 16.00, 8.00,  1.00,  12.00, 7.00,  0.00,  0.00,  3.00,  5.00,  7.00,  10.00,
  3.00,  17.00, 10.00, 18.00, 16.00, 1.00,  11.00, 18.00, 9.00,  2.00,  0.00,  12.00, 6.00,  13.00, 1.00,  13.00, 5.00,  7.00,
  13.00, 17.00, 9.00,  15.00, 13.00, 5.00,  2.00,  4.00,  4.00,  3.00,  0.00,  9.00,  11.00, 3.00,  12.00, 6.00,  11.00, 1.00,
  16.00, 12.00, 11.00, 2.00,  2.00,  15.00, 12.00, 8.00,  9.00,  14.00, 2.00,  11.00, 0.00,  0.00,  7.00,  2.00,  13.00, 15.00,
  18.00, 7.00,  16.00, 16.00, 1.00,  1.00,  12.00, 12.00, 2.00,  2.00,  1.00,  7.00,  4.00,  0.00,  8.00,  18.00, 4.00,  11.00,
  6.00,  17.00, 13.00, 12.00, 5.00,  16.00, 12.00, 0.00,  9.00,  10.00, 10.00, 18.00, 12.00, 8.00,  7.00,  5.00,  8.00,  16.00,
  3.00,  9.00,  18.00, 12.00, 13.00, 18.00, 6.00,  8.00,  12.00, 2.00,  12.00, 8.00,  8.00,  9.00,  18.00, 8.00,  0.00,  9.00,
  2.00,  6.00,  7.00,  0.00,  3.00,  11.00, 2.00,  18.00, 2.00,  16.00, 1.00,  16.00, 11.00, 11.00, 16.00, 16.00, 11.00, 7.00,
  4.00,  14.00, 10.00, 5.00,  8.00,  4.00,  14.00, 17.00, 13.00, 13.00, 3.00,  1.00,  14.00, 1.00,  7.00,  0.00,  2.00,  7.00,
  14.00, 4.00,  9.00,  14.00, 3.00,  9.00,  13.00, 13.00, 3.00,  3.00,  17.00, 0.00,  18.00, 4.00,  8.00,  6.00,  9.00,  4.00,
  2.00,  0.00,  14.00, 3.00,  3.00,  14.00, 8.00,  6.00,  7.00,  2.00,  12.00, 5.00,  14.00, 6.00,  12.00, 2.00,  16.00, 1.00,
  15.00, 7.00,  18.00, 0.00,  0.00,  13.00, 13.00, 12.00, 11.00, 16.00, 15.00, 14.00, 8.00,  9.00,  10.00, 8.00,  8.00,  13.00,
  13.00, 13.00, 10.00, 1.00,  15.00, 4.00,  0.00,  12.00, 1.00,  8.00,  12.00, 1.00,  18.00, 12.00, 18.00, 9.00,  1.00,  11.00,
  5.00,  13.00, 7.00,  1.00,  13.00, 5.00,  8.00,  17.00, 11.00, 13.00, 15.00, 9.00,  3.00,  17.00, 18.00, 9.00,  3.00,  15.00,
  11.00, 12.00, 0.00,  2.00,  15.00, 3.00,  0.00,  13.00, 1.00,  14.00, 14.00, 15.00, 8.00,  6.00,  0.00,  0.00,  11.00, 17.00,
  0.00,  1.00,  8.00,  6.00,  6.00,  10.00, 6.00,  18.00, 12.00, 7.00,  18.00, 4.00,  6.00,  15.00, 18.00, 7.00,  5.00,  8.00,
  6.00,  6.00,  7.00,  4.00,  4.00,  10.00, 17.00, 12.00, 13.00, 11.00, 15.00, 12.00, 18.00, 7.00,  12.00, 17.00, 10.00, 14.00,
  12.00, 2.00,  7.00,  17.00, 3.00,  8.00,  6.00,  3.00,  9.00,  3.00,  7.00,  7.00,  15.00, 18.00, 5.00,  13.00, 13.00, 15.00,
  10.00, 0.00,  11.00, 10.00, 1.00,  5.00,  16.00, 2.00,  7.00,  14.00, 12.00, 7.00,  17.00, 17.00, 11.00, 0.00,  5.00,  16.00,
  14.00, 1.00,  9.00,  8.00,  8.00,  3.00,  17.00, 0.00,  8.00,  6.00,  5.00,  7.00,  6.00,  17.00, 3.00,  3.00,  8.00,  3.00,
  12.00, 17.00, 5.00,  14.00, 3.00,  11.00, 5.00,  17.00, 2.00,  15.00, 1.00,  18.00, 11.00, 12.00, 0.00,  0.00,  14.00, 7.00,
  17.00, 15.00, 10.00, 18.00, 10.00, 11.00, 7.00,  12.00, 10.00, 17.00, 2.00,  18.00, 9.00,  11.00, 4.00,  17.00, 10.00, 15.00,
  12.00, 4.00,  1.00,  5.00,  10.00, 4.00,  2.00,  11.00, 3.00,  4.00,  15.00, 16.00, 10.00, 2.00,  2.00,  15.00, 16.00, 0.00,
  13.00, 16.00, 9.00,  1.00,  7.00,  3.00,  10.00, 7.00,  2.00,  12.00, 8.00,  1.00,  5.00,  0.00,  16.00, 4.00,  14.00, 13.00,
  16.00, 3.00,  0.00,  10.00, 6.00,  3.00,  9.00,  1.00,  3.00,  0.00,  13.00, 12.00, 17.00, 11.00, 4.00,  15.00, 15.00, 12.00,
  4.00,  4.00,  2.00,  16.00, 6.00,  11.00, 17.00, 14.00, 7.00,  4.00,  14.00, 8.00,  8.00,  3.00,  18.00, 17.00, 17.00, 12.00,
  10.00, 11.00, 1.00,  8.00,  13.00, 1.00,  14.00, 0.00,  9.00,  0.00,  7.00,  9.00,  0.00,  7.00,  12.00, 0.00,  18.00, 10.00,
  1.00,  5.00,  13.00, 13.00, 2.00,  10.00, 4.00,  2.00,  6.00,  0.00,  16.00, 2.00,  15.00, 13.00, 6.00,  8.00,  5.00,  6.00,
  15.00, 12.00, 6.00,  6.00,  7.00,  8.00,  1.00,  11.00, 9.00,  7.00,  18.00, 14.00, 4.00,  9.00,  6.00,  4.00,  2.00,  10.00,
  13.00, 6.00,  17.00, 2.00,  11.00, 7.00,  3.00,  8.00,  9.00,  2.00,  6.00,  18.00, 12.00, 18.00, 13.00, 8.00,  1.00,  11.00,
  4.00,  13.00, 14.00, 16.00, 8.00,  6.00,  18.00, 14.00, 15.00, 9.00,  11.00, 2.00,  13.00, 4.00,  3.00,  3.00,  15.00, 14.00,
  3.00,  13.00, 12.00, 14.00, 16.00, 18.00, 12.00, 5.00,  11.00, 2.00,  3.00,  15.00, 1.00,  12.00, 1.00,  15.00, 13.00, 12.00,
  18.00, 4.00,  17.00, 13.00, 7.00,  11.00, 13.00, 7.00,  10.00, 1.00,  3.00,  18.00, 6.00,  10.00, 3.00,  9.00,  16.00, 12.00,
  10.00, 6.00,  6.00,  7.00,  16.00, 16.00, 16.00, 17.00, 18.00, 8.00,  18.00, 0.00,  6.00,  17.00, 13.00, 14.00, 8.00,  0.00,
  7.00,  5.00,  13.00, 8.00,  12.00, 14.00, 9.00,  10.00, 10.00, 9.00,  9.00,  8.00,  6.00,  0.00,  14.00, 16.00, 8.00,  7.00,
  16.00, 10.00, 3.00,  1.00,  9.00,  11.00, 2.00,  6.00,  18.00, 5.00,  4.00,  2.00,  11.00, 10.00, 17.00, 16.00, 2.00,  12.00,
  15.00, 14.00, 6.00,  6.00,  17.00, 3.00,  10.00, 9.00,  18.00, 18.00, 6.00,  11.00, 16.00, 18.00, 17.00, 7.00,  14.00, 1.00,
  16.00, 7.00,  9.00,  11.00, 12.00, 10.00, 6.00,  1.00,  16.00, 9.00,  18.00, 0.00,  9.00,  16.00, 10.00, 14.00, 6.00,  9.00,
  14.00, 15.00, 7.00,  12.00, 8.00,  1.00,  1.00,  1.00,  17.00, 17.00, 17.00, 18.00, 1.00,  15.00, 18.00, 16.00, 16.00, 11.00,
  7.00,  4.00,  15.00, 3.00,  14.00, 13.00, 14.00, 9.00,  2.00,  4.00,  8.00,  14.00, 7.00,  0.00,  12.00, 14.00, 15.00, 8.00,
  2.00,  11.00, 5.00,  6.00,  16.00, 6.00,  4.00,  16.00, 7.00,  9.00,  5.00,  1.00,  0.00,  16.00, 10.00, 9.00,  6.00,  5.00,
  8.00,  15.00, 13.00, 7.00,  18.00, 18.00, 8.00,  0.00,  15.00, 4.00,  6.00,  14.00, 3.00,  3.00,  2.00,  9.00,  14.00, 18.00,
  13.00, 10.00, 15.00, 0.00,  11.00, 16.00, 7.00,  16.00, 18.00, 0.00,  12.00, 6.00,  13.00, 12.00, 12.00, 5.00,  3.00,  5.00,
  16.00, 13.00, 16.00, 8.00,  5.00,  12.00, 8.00,  8.00,  0.00,  2.00,  9.00,  18.00, 11.00, 8.00,  4.00,  14.00, 6.00,  17.00,
  2.00,  6.00,  8.00,  11.00, 13.00, 6.00,  18.00, 17.00, 6.00,  8.00,  0.00,  1.00,  14.00, 18.00, 4.00,  0.00,  4.00,  3.00,
  3.00,  16.00, 8.00,  16.00, 1.00,  2.00,  0.00,  15.00, 14.00, 10.00, 9.00,  15.00, 3.00,  5.00,  18.00, 5.00,  6.00,  7.00,
  3.00,  7.00,  2.00,  6.00,  12.00, 10.00, 10.00, 18.00, 16.00, 0.00,  9.00,  17.00, 10.00, 9.00,  14.00, 15.00, 5.00,  8.00,
  15.00, 3.00,  15.00, 14.00, 11.00, 6.00,  6.00,  14.00, 0.00,  14.00, 0.00,  7.00,  12.00, 8.00,  7.00,  1.00,  3.00,  6.00,
  10.00, 12.00, 1.00,  16.00, 5.00,  5.00,  6.00,  17.00, 15.00, 15.00, 9.00,  4.00,  18.00, 17.00, 13.00, 12.00, 9.00,  7.00,
  10.00, 2.00,  5.00,  8.00,  18.00, 1.00,  15.00, 8.00,  0.00,  10.00, 0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,
  0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,   0.0,
  0.0,   0.0,   0.0,   0.0,   0.0,   0.0
};

static data_t verify_data[DATA_SIZE + PADDING] = {
  126.00, 80.00,  42.00,  118.00, 58.00,  17.00,  146.00, 92.00,  122.00, 92.00,  10.00,  142.00, 139.00, 73.00,  108.00, 144.00,
  32.00,  30.00,  52.00,  73.00,  48.00,  109.00, 127.00, 66.00,  98.00,  141.00, 66.00,  77.00,  140.00, 92.00,  89.00,  25.00,
  53.00,  124.00, 95.00,  82.00,  113.00, 35.00,  18.00,  58.00,  47.00,  59.00,  148.00, 143.00, 41.00,  142.00, 125.00, 147.00,
  151.00, 34.00,  156.00, 98.00,  66.00,  111.00, 97.00,  27.00,  34.00,  139.00, 24.00,  53.00,  66.00,  20.00,  149.00, 136.00,
  47.00,  90.00,  157.00, 112.00, 41.00,  73.00,  88.00,  69.00,  39.00,  69.00,  109.00, 107.00, 49.00,  38.00,  146.00, 52.00,
  19.00,  98.00,  93.00,  71.00,  63.00,  77.00,  54.00,  62.00,  49.00,  106.00, 13.00,  160.00, 70.00,  58.00,  76.00,  88.00,
  140.00, 118.00, 38.00,  125.00, 102.00, 24.00,  5.00,   30.00,  146.00, 145.00, 130.00, 97.00,  79.00,  102.00, 160.00, 52.00,
  9.00,   10.00,  24.00,  24.00,  72.00,  89.00,  100.00, 63.00,  40.00,  120.00, 147.00, 125.00, 55.00,  74.00,  75.00,  161.00,
  58.00,  138.00, 144.00, 81.00,  155.00, 122.00, 57.00,  82.00,  128.00, 12.00,  86.00,  109.00, 65.00,  77.00,  29.00,  7.00,
  21.00,  49.00,  9.00,   79.00,  133.00, 133.00, 74.00,  60.00,  60.00,  83.00,  72.00,  137.00, 107.00, 43.00,  52.00,  78.00,
  51.00,  137.00, 48.00,  116.00, 115.00, 34.00,  114.00, 95.00,  52.00,  88.00,  89.00,  62.00,  146.00, 51.00,  120.00, 40.00,
  127.00, 106.00, 157.00, 63.00,  122.00, 47.00,  160.00, 112.00, 113.00, 9.00,   68.00,  28.00,  10.00,  58.00,  97.00,  127.00,
  124.00, 48.00,  24.00,  26.00,  20.00,  155.00, 54.00,  97.00,  125.00, 84.00,  125.00, 104.00, 12.00,  24.00,  17.00,  26.00,
  26.00,  130.00, 68.00,  152.00, 7.00,   21.00,  24.00,  144.00, 67.00,  41.00,  58.00,  68.00,  101.00, 74.00,  86.00,  120.00,
  92.00,  26.00,  12.00,  112.00, 80.00,  121.00, 18.00,  120.00, 8.00,   33.00,  130.00, 22.00,  119.00, 48.00,  139.00, 147.00,
  50.00,  90.00,  66.00,  88.00,  97.00,  56.00,  123.00, 27.00,  144.00, 152.00, 51.00,  39.00,  4.00,   126.00, 90.00,  53.00,
  128.00, 124.00, 126.00, 57.00,  21.00,  77.00,  67.00,  105.00, 78.00,  1.00,   39.00,  56.00,  74.00,  111.00, 142.00, 76.00,
  121.00, 86.00,  107.00, 9.00,   69.00,  141.00, 139.00, 147.00, 97.00,  104.00, 82.00,  36.00,  80.00,  110.00, 9.00,   52.00,
  34.00,  48.00,  46.00,  83.00,  115.00, 126.00, 80.00,  126.00, 127.00, 26.00,  36.00,  101.00, 14.00,  30.00,  28.00,  130.00,
  24.00,  57.00,  31.00,  135.00, 34.00,  16.00,  0.00,   125.00, 37.00,  36.00,  91.00,  48.00,  95.00,  38.00,  40.00,  113.00,
  122.00, 112.00, 8.00,   21.00,  133.00, 141.00, 82.00,  57.00,  87.00,  4.00,   88.00,  20.00,  9.00,   128.00, 148.00, 97.00,
  146.00, 132.00, 50.00,  73.00,  17.00,  91.00,  85.00,  13.00,  151.00, 137.00, 69.00,  61.00,  24.00,  97.00,  147.00, 85.00,
  71.00,  49.00,  27.00,  81.00,  106.00, 57.00,  75.00,  119.00, 11.00,  36.00,  40.00,  18.00,  55.00,  59.00,  32.00,  85.00,
  17.00,  118.00, 150.00, 127.00, 104.00, 14.00,  24.00,  56.00,  147.00, 17.00,  112.00, 129.00, 24.00,  22.00,  14.00,  26.00,
  126.00, 146.00, 76.00,  23.00,  50.00,  124.00, 126.00, 95.00,  66.00,  95.00,  77.00,  128.00, 142.00, 30.00,  71.00,  124.00,
  28.00,  90.00,  137.00, 76.00,  125.00, 139.00, 135.00, 132.00, 130.00, 15.00,  68.00,  49.00,  154.00, 30.00,  116.00, 90.00,
  127.00, 73.00,  83.00,  112.00, 86.00,  59.00,  121.00, 147.00, 39.00,  151.00, 47.00,  42.00,  77.00,  21.00,  117.00, 135.00,
  26.00,  40.00,  131.00, 106.00, 81.00,  21.00,  16.00,  82.00,  127.00, 14.00,  100.00, 119.00, 105.00, 129.00, 83.00,  8.00,
  149.00, 128.00, 158.00, 121.00, 49.00,  16.00,  128.00, 147.00, 129.00, 24.00,  128.00, 94.00,  21.00,  127.00, 6.00,   121.00,
  11.00,  35.00,  120.00, 115.00, 52.00,  33.00,  109.00, 150.00, 67.00,  67.00,  53.00,  57.00,  82.00,  127.00, 113.00, 154.00,
  11.00,  12.00,  136.00, 144.00, 134.00, 87.00,  145.00, 159.00, 50.00,  90.00,  90.00,  155.00, 63.00,  100.00, 50.00,  49.00,
  130.00, 34.00,  73.00,  115.00, 12.00,  113.00, 34.00,  47.00,  60.00,  124.00, 97.00,  5.00,   58.00,  148.00, 98.00,  123.00,
  147.00, 28.00,  31.00,  40.00,  50.00,  26.00,  114.00, 159.00, 112.00, 80.00,  101.00, 80.00,  41.00,  81.00,  87.00,  75.00,
  154.00, 119.00, 26.00,  68.00,  144.00, 97.00,  5.00,   80.00,  88.00,  140.00, 38.00,  13.00,  48.00,  51.00,  128.00, 122.00,
  102.00, 107.00, 113.00, 145.00, 59.00,  0.00,   21.00,  84.00,  73.00,  107.00, 52.00,  159.00, 79.00,  84.00,  108.00, 108.00,
  138.00, 96.00,  134.00, 19.00,  97.00,  150.00, 135.00, 20.00,  158.00, 40.00,  24.00,  51.00,  26.00,  25.00,  25.00,  76.00,
  122.00, 59.00,  49.00,  112.00, 125.00, 105.00, 62.00,  40.00,  89.00,  88.00,  95.00,  137.00, 8.00,   47.00,  84.00,  104.00,
  82.00,  90.00,  17.00,  101.00, 133.00, 53.00,  114.00, 34.00,  60.00,  74.00,  150.00, 16.00,  104.00, 130.00, 47.00,  53.00,
  86.00,  144.00, 85.00,  30.00,  47.00,  124.00, 150.00, 110.00, 55.00,  72.00,  89.00,  123.00, 33.00,  47.00,  66.00,  62.00,
  44.00,  113.00, 6.00,   76.00,  74.00,  18.00,  69.00,  46.00,  57.00,  10.00,  59.00,  151.00, 91.00,  144.00, 65.00,  10.00,
  86.00,  58.00,  108.00, 66.00,  141.00, 136.00, 41.00,  19.00,  84.00,  93.00,  134.00, 72.00,  152.00, 70.00,  154.00, 38.00,
  55.00,  33.00,  123.00, 2.00,   141.00, 100.00, 3.00,   115.00, 151.00, 142.00, 19.00,  157.00, 116.00, 94.00,  120.00, 50.00,
  124.00, 21.00,  35.00,  34.00,  67.00,  151.00, 1.00,   60.00,  89.00,  55.00,  37.00,  36.00,  34.00,  124.00, 121.00, 93.00,
  39.00,  19.00,  101.00, 55.00,  146.00, 9.00,   3.00,   162.00, 30.00,  34.00,  91.00,  65.00,  72.00,  100.00, 122.00, 62.00,
  134.00, 95.00,  96.00,  80.00,  64.00,  105.00, 58.00,  144.00, 98.00,  56.00,  70.00,  129.00, 29.00,  86.00,  144.00, 120.00,
  111.00, 85.00,  61.00,  8.00,   132.00, 102.00, 89.00,  98.00,  154.00, 25.00,  49.00,  144.00, 150.00, 88.00,  134.00, 40.00,
  144.00, 79.00,  152.00, 74.00,  51.00,  17.00,  41.00,  27.00,  90.00,  126.00, 34.00,  149.00, 28.00,  74.00,  67.00,  130.00,
  89.00,  128.00, 82.00,  84.00,  63.00,  118.00, 70.00,  126.00, 129.00, 3.00,   98.00,  49.00,  34.00,  114.00, 118.00, 91.00,
  144.00, 90.00,  73.00,  79.00,  150.00, 33.00,  48.00,  63.00,  73.00,  43.00,  44.00,  82.00,  62.00,  25.00,  56.00,  97.00,
  106.00, 80.00,  113.00, 40.00,  122.00, 134.00, 70.00,  17.00,  22.00,  39.00,  7.00,   140.00, 80.00,  49.00,  9.00,   1.00,
  33.00,  17.00,  65.00,  122.00, 97.00,  55.00,  162.00, 24.00,  104.00, 147.00, 95.00,  132.00, 127.00, 115.00, 86.00,  101.00,
  86.00,  145.00, 122.00, 44.00,  152.00, 30.00,  95.00,  80.00,  140.00, 158.00, 55.00,  96.00,  98.00,  99.00,  149.00, 62.00,
  64.00,  70.00,  28.00,  48.00,  31.00,  137.00, 37.00,  49.00,  16.00,  136.00, 58.00,  65.00,  134.00, 5.00,   64.00,  103.00,
  93.00,  31.00,  18.00,  130.00, 136.00, 120.00, 135.00, 100.00, 62.00,  22.00,  35.00,  67.00,  34.00,  105.00, 14.00,  74.00,
  77.00,  18.00,  23.00,  112.00, 131.00, 88.00,  71.00,  64.00,  66.00,  32.00,  68.00,  70.00,  117.00, 92.00,  52.00,  69.00,
  91.00,  21.00,  144.00, 69.00,  152.00, 48.00,  21.00,  148.00, 8.00,   152.00, 48.00,  58.00,  41.00,  50.00,  107.00, 8.00,
  148.00, 78.00,  38.00,  73.00,  2.00,   94.00,  16.00,  99.00,  149.00, 150.00, 138.00, 81.00,  94.00,  128.00, 72.00,  97.00,
  22.00,  58.00,  52.00,  8.00,   148.00, 115.00, 59.00,  144.00, 136.00, 96.00,  25.00,  106.00, 0.00,   111.00, 86.00,  154.00,
  121.00, 135.00, 35.00,  93.00,  138.00, 125.00, 70.00,  135.00, 91.00,  111.00, 98.00,  14.00,  116.00, 122.00, 26.00,  106.00,
  16.00,  136.00, 97.00,  113.00, 58.00,  41.00,  46.00,  103.00, 109.00, 88.00,  31.00,  83.00,  127.00, 14.00,  59.00,  150.00,
  86.00,  70.00,  112.00, 110.00, 72.00,  39.00,  140.00, 144.00, 71.00,  113.00, 75.00,  6.00,   42.00,  132.00, 105.00, 80.00,
  109.00, 109.00, 70.00,  137.00, 63.00,  103.00, 41.00,  20.00,  66.00,  57.00,  125.00, 52.00,  145.00, 103.00, 98.00,  138.00,
  37.00,  112.00, 74.00,  129.00, 111.00, 64.00,  144.00, 106.00, 0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,
  0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,
  0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0
};