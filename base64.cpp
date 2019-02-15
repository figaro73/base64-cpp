#include <cstring>

#include <iostream>
#include <string>
#include <memory>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

enum Encoding { ENCODE, DECODE };

using allocated_char_ptr = std::unique_ptr< char, decltype( &::free ) >;

bool base64( enum Encoding p_operation, const char *p_input, allocated_char_ptr & p_output_ptr, size_t & p_output_length ) {

  if( p_input == NULL || strlen( p_input ) == 0 ) {
    p_output_ptr.reset( NULL );
    p_output_length = 0;
    return false;
  }
 
  const char      *l_input = p_input;
  allocated_char_ptr l_input_str_ptr( NULL, &::free );

  if( p_input == p_output_ptr.get(  ) ) {
    l_input_str_ptr.reset( ( char * ) calloc( strlen( p_input ) + 1, sizeof( char ) ) );
    strncpy( l_input_str_ptr.get(  ), p_input, strlen( p_input ) );
    l_input = l_input_str_ptr.get(  );
  }

  size_t          l_input_length = strlen( p_input ), l_output_buffer_length = 0;
  char            *l_output_buffer = NULL;
  p_output_ptr.reset( NULL );

  allocated_char_ptr l_tmp_base64_str_ptr( NULL, &::free );
  //removing \n characters from base64 formatted string, calculate the decoding output buffer length for base64 decoding
  if( p_operation == Encoding::DECODE ) {
    l_tmp_base64_str_ptr.reset( ( char * ) calloc( l_input_length + 1, sizeof( char ) ) );
    char           *l_p = l_tmp_base64_str_ptr.get(  );
    //remove newline from formatted base64 string
    strncpy( l_p, l_input, l_input_length );
    if( l_input_length > 64 && l_input[64] == '\n' ) {
      for( size_t l_i = 0; l_i < l_input_length; l_i += 65 ) {
        size_t          l_remain = l_input_length - l_i;
        strncpy( l_p, l_input + l_i, l_remain > 64 ? 64 : l_remain );
        if( l_remain < 65 && *( l_p + l_remain - 1 ) == '\n' ) {
          *( l_p + l_remain - 1 ) = 0;
        }
        l_p += ( l_remain > 65 ? 64 : l_remain );
      }
      *( ++l_p ) = 0;
    } else if( l_input_length <= 64 && l_input[l_input_length - 1] == '\n' ) {
      l_p[l_input_length - 1] = 0;
    }

    l_input = l_tmp_base64_str_ptr.get(  );
    l_input_length = strlen( l_input );
    size_t          l_padding = 0;
    if( l_input[l_input_length - 1] == '=' && l_input[l_input_length - 2] == '=' ) {
      l_padding = 2;
    } else if( l_input[l_input_length - 1] == '=' ) {
      l_padding = 1;
    }
    l_output_buffer_length = ( l_input_length * 3 ) / 4 - l_padding;
  }

/*
  BIO chain: l_b64 - l_bio. ENCODE: write in l_b64 and read from l_bio, DECODE: write in l_bio and read from l_b64
*/

  std::unique_ptr < BIO, decltype( &::BIO_free_all ) > l_b64_ptr( BIO_new( BIO_f_base64(  ) ), &::BIO_free_all );
  BIO            *l_b64 = l_b64_ptr.get(  );
  BIO_set_flags( l_b64, BIO_FLAGS_BASE64_NO_NL );       //on decoding if this flag is not set, after BIO_write needs BIO_write( l_bio, "\n", 1 );
  BIO            *l_bio = BIO_new( BIO_s_mem(  ) );
  BIO_push( l_b64, l_bio );

  if( p_operation == Encoding::ENCODE ) {
    if( BIO_write( l_b64, l_input, strlen( l_input ) ) > 0 ) {
      if( BIO_flush( l_b64 ) != 1 ) {
        return false;
      }
    }

    BUF_MEM        *l_buf_mem = NULL;
    BIO_get_mem_ptr( l_bio, &l_buf_mem );
    l_output_buffer_length = l_buf_mem->length;
    l_output_buffer = ( char * ) calloc( l_output_buffer_length + 1, sizeof( char ) );
    if( l_output_buffer == NULL ) {
      return false;
    }
    p_output_length = BIO_read( l_bio, l_output_buffer, l_output_buffer_length );
  } else {
    if( BIO_write( l_bio, l_input, strlen( l_input ) ) > 0 ) {
      if( BIO_flush( l_bio ) != 1 ) {
        return false;
      }
    }
    l_output_buffer = ( char * ) calloc( l_output_buffer_length + 1, sizeof( char ) );
    if( l_output_buffer == NULL ) {
      return false;
    }
    p_output_length = BIO_read( l_b64, l_output_buffer, l_output_buffer_length );
  }

  p_output_ptr.reset( l_output_buffer );

  if( BIO_set_close( l_bio, BIO_CLOSE ) != 1 ) {
    return false;
  }
  if( BIO_set_close( l_b64, BIO_CLOSE ) != 1 ) {
    return false;
  }

  return true;
}


int main(  ) {

  allocated_char_ptr l_output( NULL, &::free );
  size_t l_output_length = 0;
  const char     *l_input_str = "test string";

  std::cout << "Original string: " << l_input_str << std::endl;
  if( base64( Encoding::ENCODE, l_input_str, l_output, l_output_length ) ) {
    std::cout << "base64 encoded result: " << l_output.get(  ) << std::endl;

  } else {
    std::cout << "base64 encode error!" << std::endl;
    return 1;
  }

  l_input_str = l_output.get(  );
  std::cout << "Original base64 string: " << l_input_str << std::endl;
  if( base64( Encoding::DECODE, l_input_str, l_output, l_output_length ) ) {
    std::cout << "base64 decoded result: " << l_output.get(  ) << std::endl;
  } else {
    std::cout << "base64 decode error!" << std::endl;
    return 1;
  }

  return 0;
}

