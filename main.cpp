#include <stdio.h>
#include <string.h>


unsigned char buffer[0x100000];
unsigned char bufout[0x100000];
int buf_size, bufout_size;


int packer_code[0x100000];
int packer_ptr[0x100000];
int codes_size;


void unpack( char *fp_name )
{
	FILE *fp = fopen( fp_name, "rb" );
	buf_size = fread( buffer, 1, 0x100000, fp );
	fclose( fp );


	char str[512];

	fp_name[strlen(fp_name) - 4] = 0;
	sprintf( str, "%s.unp", fp_name );

	fp = fopen( str, "wb" );
	fwrite( buffer, 1, 0x80, fp );


	bufout_size = 0;
	for( int lcv = 0x80; lcv < buf_size; lcv += 8 ) {
		int pixel8;
		int plane1, plane2, plane3, plane4;


		plane4 = buffer[ lcv+0 ];
		plane3 = buffer[ lcv+2 ];
		plane2 = buffer[ lcv+4 ];
		plane1 = buffer[ lcv+6 ];

		pixel8 = 0;
		for( int lcv2 = 0; lcv2 < 8; lcv2++ ) {
			pixel8 <<= 1; pixel8 |= ( plane1 & 0x80 ) >> 7; plane1 <<= 1;
			pixel8 <<= 1; pixel8 |= ( plane2 & 0x80 ) >> 7; plane2 <<= 1;
			pixel8 <<= 1; pixel8 |= ( plane3 & 0x80 ) >> 7; plane3 <<= 1;
			pixel8 <<= 1; pixel8 |= ( plane4 & 0x80 ) >> 7; plane4 <<= 1;
		}

		bufout[ bufout_size+0 ] = ( pixel8 >> 24 ) & 0xff;
		bufout[ bufout_size+1 ] = ( pixel8 >> 16 ) & 0xff;
		bufout[ bufout_size+2 ] = ( pixel8 >> 8 ) & 0xff;
		bufout[ bufout_size+3 ] = ( pixel8 >> 0 ) & 0xff;



		plane4 = buffer[ lcv+1 ];
		plane3 = buffer[ lcv+3 ];
		plane2 = buffer[ lcv+5 ];
		plane1 = buffer[ lcv+7 ];

		pixel8 = 0;
		for( int lcv2 = 0; lcv2 < 8; lcv2++ ) {
			pixel8 <<= 1; pixel8 |= ( plane1 & 0x80 ) >> 7; plane1 <<= 1;
			pixel8 <<= 1; pixel8 |= ( plane2 & 0x80 ) >> 7; plane2 <<= 1;
			pixel8 <<= 1; pixel8 |= ( plane3 & 0x80 ) >> 7; plane3 <<= 1;
			pixel8 <<= 1; pixel8 |= ( plane4 & 0x80 ) >> 7; plane4 <<= 1;
		}

		bufout[ bufout_size+0+32 ] = ( pixel8 >> 24 ) & 0xff;
		bufout[ bufout_size+1+32 ] = ( pixel8 >> 16 ) & 0xff;
		bufout[ bufout_size+2+32 ] = ( pixel8 >> 8 ) & 0xff;
		bufout[ bufout_size+3+32 ] = ( pixel8 >> 0 ) & 0xff;



		bufout_size += 4;
		if( (bufout_size % 32) == 0 ) {
			bufout_size += 32;
		}
	}


	fwrite( bufout, 1, bufout_size, fp );
	fclose( fp );
}


void repack( char *fp_name )
{
	FILE *fp = fopen( fp_name, "rb" );
	buf_size = fread( buffer, 1, 0x100000, fp );
	fclose( fp );


	char str[512];

	fp_name[strlen(fp_name) - 4] = 0;
	sprintf( str, "%s.rep", fp_name );

	fp = fopen( str, "wb" );


	bufout_size = 0;
	codes_size = 0;


	int lcv = 0;
	while( lcv < buf_size ) {
		int run_size = 0;
		int run_byte = buffer[lcv];
		int run_ptr = lcv;


		// run-length encoding (rle)
		if( buffer[lcv] == buffer[lcv+1] && lcv < buf_size-1 ) {
			while( lcv < buf_size ) {
				if( run_byte != buffer[lcv] ) { break; }
				lcv++;


				run_size++;
				if( run_size == 127 ) { break; }
			}

			packer_ptr[ codes_size ] = run_ptr;
			packer_code[ codes_size++ ] = run_size;
			continue;
		}


		// raw-length encoding
		while( lcv < buf_size-1 ) {
			// rle-2 = quit
			if( buffer[lcv] == buffer[lcv+1] ) { break; }
			lcv++;


			run_size++;
			if( run_size == 127-1 ) { break; }
		}

		if( lcv == buf_size-1 && run_size < 127-1 ) {
			lcv++;
			run_size++;
		}


		packer_ptr[ codes_size ] = run_ptr;
		packer_code[ codes_size++ ] = run_size | 0x80;
	}



	for( lcv = 0; lcv < codes_size; lcv++ ) {
		// combine raw-x + rle-2 + raw-x  [save 1 byte]
		if( ( packer_code[lcv+0] >= 0x80 ) &&
			 ( packer_code[lcv+1] == 0x02 ) &&
			 ( packer_code[lcv+2] >= 0x80 ) &&
			 lcv < codes_size-3 )
		{
			int run_size = 0;

			run_size += packer_code[lcv+0] & 0x7f;
			run_size += packer_code[lcv+1] & 0x7f;
			run_size += packer_code[lcv+2] & 0x7f;

			if( run_size >= 127-1 ) { continue; }

			packer_code[lcv+2] = run_size | 0x80;
			packer_code[lcv+1] = -1;
			packer_code[lcv+0] = -1;

			packer_ptr[lcv+2] = packer_ptr[lcv+0];
		}
	}



	int run_ptr = 0;
	for( lcv = 0; lcv < codes_size; lcv++ ) {
		if( packer_code[lcv] == -1 ) continue;


		// rle
		if( packer_code[lcv] < 0x80 ) {
			fputc( packer_code[lcv], fp );
			fputc( buffer[ packer_ptr[lcv] ], fp );

			continue;
		}


		// raw
		fputc( packer_code[lcv], fp );
		fwrite( buffer + packer_ptr[lcv], 1, packer_code[lcv] & 0x7f, fp );
	}


	// end-of-file (eof)
	fputc( 0xff, fp );

	fclose( fp );
}


int main(int argc, char **argv)
{
	if( argc < 2 ) {
		printf( "Error: drag-and-drop multiple files\n" );
		scanf( "%s" );
		return -1;
	}


	for( int lcv = 1; lcv < argc; lcv++ ) {
		if( strstr( argv[lcv], ".bin======" ) != 0 ) {
			unpack( argv[lcv] );
		}

		else if( strstr( argv[lcv], ".nam" ) != 0 ) {
			repack( argv[lcv] );
		}
	}


	return 0;
}
