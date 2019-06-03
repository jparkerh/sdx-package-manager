#include "hls_math.h"
#include "types_cv.h"
#include <ap_fixed.h>

#include "common/xf_common.h"
#include "common/xf_infra.h"
#include "common/xf_video_mem.h"

#define FILTER_OFFS 2
#define FILTER_SIZE 5
#define width 1280
#define height 720

typedef ap_axiu<24,1,1,1> VIDEO;
typedef hls::stream<VIDEO> VIDEO_STREAM;
typedef xf::Scalar<3, unsigned char> RGB_PIXEL;
typedef xf::Scalar<1, unsigned char> G_PIXEL;
typedef xf::Mat<XF_8UC3, height, width, XF_NPPC1> RGB_IMAGE;
typedef xf::Mat<XF_8UC1, height, width, XF_NPPC1> G_IMAGE;
typedef xf::Window<FILTER_SIZE, FILTER_SIZE, G_PIXEL> G_WINDOW;

void hls_2DFilter(G_IMAGE&, G_IMAGE&);
G_PIXEL sobel_operator(G_WINDOW&);

// Generic 2D filtering function with source for border pixels
void hls_2DFilter(G_IMAGE& input_mat, G_IMAGE& output_mat) {
	G_WINDOW IN_WINDOW;
	G_PIXEL ip_pixel;
	G_PIXEL sp_pixel;

	// Using line buffers larger than the frame allows for filling the "border"
	// of the image. This keeps from adding additional complexity to the image
	// process.
	G_PIXEL line_buffer[width + (FILTER_OFFS * 2)][FILTER_SIZE];

	// Partition the array along the second dimension. This means that a full
	// column of the linebuffer is available to the process at once.
	#pragma HLS ARRAY_PARTITION variable=line_buffer complete dim=2

	// L1 will loop over the rows of the image, while L2 loops over the columns.
	// As there are w * h pixels in the image, we need to consume that many pixels
	// within these loops.
	L1: for(int row = 0; row < height + FILTER_OFFS + 1; row++)
	{
	L2: for(int col = 0; col < width; col++)
	{
#pragma HLS LOOP_FLATTEN off
#pragma HLS PIPELINE II=1

		// We can only consume w * h pixels, so bound by height. This
		// accounts for the cycles when we are sending pixels out but
		// not taking any in.
		if (row < height) {

			// Pull the input pixel from the input mat. Using this style
			// allows for a dataflow to be applied, pipelining the data through
			// multiple separate filters.
			ip_pixel = input_mat.data[(row * width) + col];

			// These conditional statements will fill the line buffer while
			// accounting for the border pixels of the image. This filter
			// is a 3x3 filter, which means that the 1 pixel offset needs
			// to be applied to the top, bottom, and sides of the image.

			// If we are on either side of the image, we can copy the pixel in
			// that column out into the extended line buffer, creating the buffer
			if (col == 0) {
				for (int border_col = 0; border_col < FILTER_OFFS; border_col++) {
#pragma HLS UNROLL
					line_buffer[border_col][FILTER_SIZE-1] = ip_pixel;
				}
			} else if (col == (width - 1)) {
				for (int border_col = 0; border_col < FILTER_OFFS; border_col++) {
#pragma HLS UNROLL
					line_buffer[width - border_col][FILTER_SIZE-1] = ip_pixel;
				}
			} else {
				line_buffer[col + FILTER_OFFS][FILTER_SIZE-1] = ip_pixel;
			}


		}


		// For all columns, shift the window to the left and then fill from
		// the bordered line buffer to obtain the correct window
		if (row > FILTER_OFFS) {
			IN_WINDOW.shift_pixels_left();
			for (int window_row = 0; window_row < FILTER_SIZE; window_row++) {
#pragma HLS UNROLL

				IN_WINDOW.insert_pixel(line_buffer[col + FILTER_OFFS][window_row], window_row, FILTER_SIZE-1);
			}
			sp_pixel = sobel_operator(IN_WINDOW);

			output_mat.data[(row * width) + col] = sp_pixel.val[0];

		}

		// The vertical portion of the border was not taken care of on the
		// input side of the image, but will be taken care of on the line
		// buffer shift up for the image by copying the pixel all the way up
		// from the bottom of the line buffer.

		// TODO: Fix this logic so that it will take care of corner bordering as well.
		if (row == 0) {
			for (int fil = 0; fil < FILTER_SIZE-1; fil++){
#pragma HLS UNROLL
				line_buffer[col + FILTER_OFFS][fil] = line_buffer[col + FILTER_OFFS][FILTER_SIZE - 1];
			}
		} else {
			for (int fil = 0; fil < FILTER_SIZE-1; fil++){
#pragma HLS UNROLL
				line_buffer[col + FILTER_OFFS][fil] = line_buffer[col + FILTER_OFFS][fil+1];
			}
		}

	}
	}
}

void rgb2gry(RGB_IMAGE& input_mat, G_IMAGE& output_mat) {
	ap_uint<24> input_pixel;
	uint8_t red;
	uint8_t green;
	uint8_t blue;

	for (int _row=0; _row < height; _row++){
		for (int _col=0; _col < width; _col++){
#pragma HLS pipeline ii=1
			input_pixel = input_mat.data[(_row*width) + _col];
			red = input_pixel.range(7,0);
			green = input_pixel.range(15,8);
			blue = input_pixel.range(23,16);

			output_mat.data[(_row*width) + _col] = (red/8) + (green/8) + (blue/8);
		}
	}
}

void gry2rgb(G_IMAGE& input_mat, RGB_IMAGE& output_mat) {
	ap_uint<24> output_pixel;
	ap_uint<8> input_pixel;

	for (int _row=0; _row < height; _row++){
		for (int _col=0; _col < width; _col++){
#pragma HLS pipeline ii=1
			input_pixel = input_mat.data[(_row*width) + _col];
			output_pixel.range(7,0) = input_pixel;
			output_pixel.range(15,8) = input_pixel;
			output_pixel.range(23,16) = input_pixel;
			output_mat.data[(_row*width) + _col] = output_pixel;
		}
	}
}

void strm2mat(VIDEO_STREAM& IN, RGB_IMAGE& OUT) {
	for (int i = 0; i < OUT.size; i++) {
		OUT.data[i] = IN.read().data;
	}
}

void mat2strm(RGB_IMAGE& IN, VIDEO_STREAM& OUT) {
	ap_axiu<24,1,1,1> tmp;
	for (int i = 0; i < IN.size; i++) {
		tmp.data = IN.data[i];
		OUT.write(tmp);
	}
}


void hls_video_block (VIDEO_STREAM& INPUT_STREAM, VIDEO_STREAM& OUTPUT_STREAM) {
#pragma HLS INTERFACE axis port=OUTPUT_STREAM bundle=VIDEO_OUT
#pragma HLS INTERFACE axis port=INPUT_STREAM bundle=VIDEO_IN
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS DATAFLOW

	RGB_IMAGE in_tmp;
	G_IMAGE im_1;
	G_IMAGE im_2;
	RGB_IMAGE out_tmp;

#pragma HLS STREAM variable=in_tmp depth=100
#pragma HLS STREAM variable=im_1 depth=100
#pragma HLS STREAM variable=im_2 depth=100
#pragma HLS STREAM variable=out_tmp depth=100

	//Convert AXI video stream to mat
	strm2mat(INPUT_STREAM, in_tmp);

	//Perform operation on "CV-style" mat
	rgb2gry(in_tmp, im_1);
	hls_2DFilter(im_1, im_2);
	gry2rgb(im_2, out_tmp);

	//Convert mat to AXI video stream
	mat2strm(out_tmp, OUTPUT_STREAM);

}

// Simple sobel operator, outputs a binary edge "detected"
G_PIXEL sobel_operator(G_WINDOW& input_window){
	G_PIXEL temp_pixel;

	char kernelx[5][5] = { { 2, 2, 4, 2, 2 },
	    { 1, 1, 2, 1, 1 },
	    { 0, 0, 0, 0, 0 },
		{ -1, -1, -2, -1, -1},
		{ -2, -2, -4, -2, -2} };

	char kernely[5][5] = { { 2, 1, 0, -1, -2 },
		    { 2, 1, 0, -1, -2 },
		    { 4, 2, 0, -2, -4 },
			{ 2, 1, 0, -1, -2},
			{ 2, 1, 0, -1, -2} };

	char x_mag, y_mag;

	x_mag = 0;
	y_mag = 0;

	// These loops are unrolled to provide "1 pixel per clock" operation
	hor_conv: for (int rowOffset = -1; rowOffset <= 1; rowOffset++){
#pragma HLS UNROLL
		ver_conv: for (int colOffset = -1; colOffset <= 1; colOffset++){
#pragma HLS UNROLL
			temp_pixel = input_window.getval(1 + rowOffset, 1 + colOffset);
			x_mag = x_mag + (temp_pixel.val[0] * kernelx[1 + rowOffset][1 + colOffset]);
			y_mag = y_mag + (temp_pixel.val[0] * kernely[1 + rowOffset][1 + colOffset]);
		}
	}

	// Binary output: if conv > 200 output 255.
	if ((hls::abs(x_mag) + hls::abs(y_mag)) > 200){
		return 255;
	} else {
		return 0;
	}

	return 0;

}
