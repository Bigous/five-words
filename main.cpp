#include <algorithm>
#include <array>
#include <chrono>
#include <execution>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <boost/iostreams/device/mapped_file.hpp>

struct Frequency {
	std::uint32_t count;
	std::uint8_t  letter;
};

inline constexpr std::uint8_t count_bit_set( auto value )
{
	std::uint8_t count;
	for( count = 0; value; ++count ) {
		value &= ( value - 1 );
	}
	return count;
}

inline constexpr unsigned trailing_zeroes( int n )
{
	unsigned bits = 0, x = n;

	if( x ) {
		/* assuming `x` has 32 bits: lets count the low order 0 bits in batches */
		/* mask the 16 low order bits, add 16 and shift them out if they are all 0
		 */
		while( !( x & 0x0000FFFF ) ) {
			bits += 16;
			x >>= 16;
		}
		/* mask the 8 low order bits, add 8 and shift them out if they are all 0 */
		if( !( x & 0x000000FF ) ) {
			bits += 8;
			x >>= 8;
		}
		/* mask the 4 low order bits, add 4 and shift them out if they are all 0 */
		if( !( x & 0x0000000F ) ) {
			bits += 4;
			x >>= 4;
		}
		/* mask the 2 low order bits, add 2 and shift them out if they are all 0 */
		if( !( x & 0x00000003 ) ) {
			bits += 2;
			x >>= 2;
		}
		/* mask the low order bit and add 1 if it is 0 */
		bits += ( x & 1 ) ^ 1;
	}
	return bits;
}

void read_words( // mm_vector<char> &file,
		std::string_view &file, std::unordered_map< std::uint32_t, std::size_t > &bits_to_index,
		std::vector< std::uint32_t > &index_to_bits, std::vector< std::string_view > &index_to_words,
		std::array< std::vector< std::uint32_t >, 26 > &letter_to_words_bits, std::array< std::uint32_t, 26 > &letter_mask )
{

	auto start = std::chrono::high_resolution_clock::now();

	std::array< Frequency, 26 > freq;
	for( std::uint8_t i = 0; auto &[count, letter]: freq ) {
		count  = 0;
		letter = i;
		++i;
	}

	// read words
	std::size_t   word_begin = 0;
	std::uint32_t bits       = 0;
	for( std::size_t i = 0; i < file.size(); ++i ) {
		const char &ch = file[i];

		if( ch != '\n' ) {
			bits |= 1 << ( ch - 'a' );
			continue;
		}

		std::size_t len       = i - word_begin;
		auto        this_bits = bits;
		word_begin            = i + 1;
		bits                  = 0;

		// skip words without 5 leters
		if( len != 5 )
			continue;

		// skip words with repeated leters
		if( count_bit_set( this_bits ) != 5 )
			continue;

		// skip anagrams
		if( bits_to_index.contains( this_bits ) )
			continue;

		// count leter frequency
		std::string_view word{ &file[i - len], len };
		for( auto c: word ) {
			std::size_t index = c - 'a';
			++freq[index].count;
		}

		bits_to_index.insert( std::make_pair( this_bits, index_to_bits.size() ) );
		index_to_bits.push_back( this_bits );
		index_to_words.push_back( word );
	}

	auto checkpoint = std::chrono::high_resolution_clock::now();

	std::cout << std::chrono::duration_cast< std::chrono::microseconds >( checkpoint - start ).count()
						<< "us Ingested file\n";

	// rearrange letter order based on letter frequency (least used letter gets
	// lowest index)
	std::ranges::sort(
			freq, []( const auto &a, const auto &b ) -> auto{ return a.count < b.count; } );

	std::array< std::size_t, 26 > reverse_letter_order;

	for( int i = 0; i < 26; ++i ) {
		letter_mask[i]                       = static_cast< std::uint32_t >( 1 ) << freq[i].letter;
		reverse_letter_order[freq[i].letter] = i;
	}

	for( auto w: index_to_bits ) {
		auto        m   = w;
		std::size_t min = 26;

		while( m != 0 ) {
			auto letter = trailing_zeroes( m );
			min         = std::min( min, reverse_letter_order[letter] );

			m ^= 1 << letter;
		}

		letter_to_words_bits[min].push_back( w );
	}
}

auto find_words_parallel( const std::array< std::uint32_t, 26 >                  &letter_mask,
                          const std::array< std::vector< std::uint32_t >, 26 >   &letter_to_words_bits,
                          const std::unordered_map< std::uint32_t, std::size_t > &bits_to_index,
                          const std::vector< std::string_view >                  &index_to_words ) -> std::size_t;

constexpr const char *file_name = "words_alpha.txt";

auto main() -> int
{
	using clock = std::chrono::high_resolution_clock;
	using us    = std::chrono::microseconds;
	using std::chrono::duration_cast;

	try {

		std::unordered_map< std::uint32_t, std::size_t > bits_to_index{};
		std::vector< std::uint32_t >                     index_to_bits{};
		std::vector< std::string_view >                  index_to_words{};
		std::array< std::vector< std::uint32_t >, 26 >   letter_to_words_bits{};
		std::array< std::uint32_t, 26 >                  letter_mask{ 0 };

		auto begin = clock::now();

		boost::iostreams::mapped_file mm_file( file_name, std::ios::binary | std::ios::in );

		// We don't need the string_view. Coult use the mm_file directly, but it's easyer to use strings and string_view is
		// very cheap. Could be std::span too.
		std::string_view file_content{ mm_file.const_data(), mm_file.size() };

		read_words( file_content, bits_to_index, index_to_bits, index_to_words, letter_to_words_bits, letter_mask );

		auto read_time = duration_cast< us >( clock::now() - begin ).count();

		auto mid = clock::now();

		auto solutions = find_words_parallel( letter_mask, letter_to_words_bits, bits_to_index, index_to_words );

		auto finished_now = clock::now();
		auto process_time = duration_cast< us >( finished_now - mid ).count();

		auto total_time = duration_cast< us >( finished_now - begin ).count();

		std::cout << read_time << "us Reading time\n"
							<< process_time << "us Processing time\n"
							<< total_time << "us Total time\n"
							<< "Found " << solutions << " solutions" << std::endl;
	} catch( const std::exception &exception ) {
		std::cerr << "Unhandled exception: " << exception.what() << std::endl;
	}
	return 0;
}

void print_output( const std::vector< std::string_view > &index_to_words, std::array< std::size_t, 5 > &words )
{
	// doint it in a string because it's called from inside threads
	std::stringstream ss;
	for( const auto &w: words ) {
		ss << index_to_words[w] << ' ';
	}
	ss << '\n';
	std::cout << ss.str();
}

auto find_words( const std::array< std::uint32_t, 26 >                  &letter_mask,
                 const std::array< std::vector< std::uint32_t >, 26 >   &letter_to_words_bits,
                 const std::unordered_map< std::uint32_t, std::size_t > &bits_to_index,
                 const std::vector< std::string_view >                  &index_to_words,

                 const std::uint32_t total_bits, const std::size_t num_words, std::array< std::size_t, 5 > &words,
                 const std::size_t max_letter, int skipped ) -> std::size_t
{

	std::size_t num_solutions = 0;

	// If we don't have 5 letters left there is not point going on
	constexpr const std::size_t upper = 26 - 5;

	// walk over all letters in a certain order until we find an unused one
	for( std::size_t i = max_letter; i < upper; i++ ) {
		int m = letter_mask[i];
		if( ( total_bits & m ) != 0 )
			continue;

		// take all words from the index of this letter and add each word to the
		// solution if all letters of the word aren't used before.

		// Use parallelism at the top level only
		if( num_words == 0 || num_words == 1 ) {
			num_solutions += std::transform_reduce(
					std::execution::par_unseq, letter_to_words_bits[i].cbegin(), letter_to_words_bits[i].cend(), std::size_t{ 0 },
					std::plus{}, [&]( const std::uint32_t &w ) -> std::size_t {
						if( ( total_bits & w ) != 0 ) {
							return 0;
						}
						auto                         idx = bits_to_index.find( w )->second;
						std::array< std::size_t, 5 > new_words{ words };
						new_words[num_words] = idx;
						return find_words( letter_mask, letter_to_words_bits, bits_to_index, index_to_words, total_bits | w,
				                       num_words + 1, new_words, i + 1, skipped );
					} );
		} else {
			if( num_words == 4 && skipped >= 0 ) {
				const int candidate = ( ~( total_bits | letter_mask[skipped] ) ) & 0x3FFFFFF;
				if( auto iter = bits_to_index.find( candidate ); iter != bits_to_index.end() ) {
					words[num_words] = iter->second;
					print_output( index_to_words, words );
					++num_solutions;
				}
			} else {
				for( const auto &w: letter_to_words_bits[i] ) {
					if( ( total_bits & w ) != 0 )
						continue;

					const std::size_t idx = bits_to_index.find( w )->second;
					words[num_words]      = idx;

					if( num_words == 4 ) {
						print_output( index_to_words, words );
						++num_solutions;
					} else {
						num_solutions += find_words( letter_mask, letter_to_words_bits, bits_to_index, index_to_words,
						                             total_bits | w, num_words + 1, words, i + 1, skipped );
					}
				}
			}
		}

		if( skipped >= 0 ) {
			break;
		}
		skipped = static_cast< int >( i );
	}
	return num_solutions;
}

auto find_words_parallel( const std::array< std::uint32_t, 26 >                  &letter_mask,
                          const std::array< std::vector< std::uint32_t >, 26 >   &letter_to_words_bits,
                          const std::unordered_map< std::uint32_t, std::size_t > &bits_to_index,
                          const std::vector< std::string_view >                  &index_to_words ) -> std::size_t
{
	std::array< std::size_t, 5 > words{ 0 };
	return find_words( letter_mask, letter_to_words_bits, bits_to_index, index_to_words, 0, 0, words, 0, -1 );
}