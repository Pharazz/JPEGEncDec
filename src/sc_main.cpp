#include <systemc.h>
#include "functions.h"
#include "fdct.h"
#include "idct.h"

//#include inverse module

#define NS *1e-9 // use this constant to make sure that the clock signal is in nanoseconds

int sc_main(int argc, char *argv[]) {
	char choice;
	unsigned char i, j;
	sc_signal<FILE *> sc_input; 	// input file pointer signal
	sc_signal<FILE *> sc_output;	// output file pointer signal
	sc_signal<double> dct_data[8][8], idct_data[8][8]; // signal to the dc transformed data
	sc_signal<double> cosine_tbl[8][8]; // signal for the cosine table values

	sc_signal<bool> clk1, clk2; 		// clock signal for FDCT (also need a 2nd clock for IDCT)

	FILE *input, *output; // input and output file pointers
	double cosine[8][8]; // the cosine table
	double data[8][8]; // the data read from the signals, which will be zigzagged
	
	//Decompression signals and variables




	if (argc == 4) {
		if (!(input = fopen(argv[1], "rb"))) // error occurred while trying to open file
			printf("\nSystemC JPEG LAB:\ncannot open file '%s'\n", argv[1]), exit(1);

		if (!(output = fopen(argv[2], "wb"))) // rror occurred while trying to create file
			printf("\nSystemC JPEG LAB:\ncannot create file '%s'\n", argv[2]), exit(1);

		// copy the input and output file pointer onto the respective ports
		sc_input.write(input);//input file
		sc_output.write(output);//output file

		choice = *argv[3];
	} else
		fprintf(stderr, "\nSystemC JPEG LAB: insufficient command line arguments\n"
			"usage:  ./sc_jpeg.x [input file] [output file] [(C)ompress or (D)ecompress]\n")
				, exit(1);

	 // write the header, read from the input file
	write_read_header(input, output);
	sc_input.write(output);
	 // make the cosine table
	make_cosine_tbl(cosine);

	for (i = 0; i < 8; i++){
		for(j = 0;j < 8; j++){
			cosine_tbl[i][j].write(cosine[i][j]);
		}
	}

	// call the forward discrete transform module
	fdct fdct{"FDCT"};
	fdct.clk(clk1);
	fdct.sc_input(sc_input);
	for (i = 0; i < 8; i++){
			for(j = 0;j < 8; j++){
				fdct.fcosine[i][j](cosine_tbl[i][j]);
				fdct.out64[i][j](dct_data[i][j]);
			}
	}

	//fdct.fcosine(cosine_tbl);
	// and bind the ports -FDCT


	// copy the cosine table and the quantization table onto the corresponding signals to send to DCT module

	//binds ports - idct
	idct idct{"IDCT"};
	idct.clk(clk2);
	idct.sc_input(sc_output);
	for (i = 0; i < 8; i++){
			for(j = 0;j < 8; j++){
				idct.fcosine[i][j](cosine_tbl[i][j]);
				idct.input_data[i][j](idct_data[i][j]);
			}
	}
	

	// because compression and decompression are two different processes, we must use
	// two different clocks, to make sure that when we want to compress, we only compress
	// and dont decompress by mistake
	sc_start(SC_ZERO_TIME); 	// initialize the clock
	if ((choice == 'c') || (choice == 'C')) { // for compression
		while (!(feof(input))) { // cycle the clock for as long as there is something to be read from the input file
			// create the FDCT clock signal
			clk1.write(1);		// convert the clock to high
			sc_start(10, SC_NS);	// cycle the high for 10 nanoseconds

			clk1.write(0);		// start the clock as low
			sc_start(10, SC_NS);	// cycle the low for 10 nanoseconds
			for (i = 0; i < 8; i++)
				for(j = 0;j < 8; j++)
					data[i][j] = dct_data[i][j].read();


			// read back signals from module
			zigzag_quant(data, output);

			// zigzag and quantize the outputted data - will write out to file (see functions.h)



		}
	} else if ((choice == 'd') || (choice == 'D')) { // for decompression
		while (!(feof(input))) {
			//unzigzag and inverse quatize input file and result will be placed in data
			unzigzag_iquant(data, output);
			
			//write unzigzag data to ports
			for (i = 0; i < 8; i++){
				for(j = 0;j < 8; j++){
					idct_data[i][j].write(data[i][j]);
				}
			}
		
			clk2.write(1);		// convert the clock to high
			sc_start(10, SC_NS);	// cycle the high for 10 nanoseconds
			clk2.write(0);		// start the clock as low
			sc_start(10, SC_NS);	// cycle the low for 10 nanoseconds
			for (i = 0; i < 8; i++){
				for(j = 0;j < 8; j++){
					data[i][j] = idct_data[i][j].read();
				}
			}
			 //read idct data from ports & write to output file


		}
		
	}	
	fclose(sc_input.read());
	fclose(sc_output.read());

	return 0;
}
