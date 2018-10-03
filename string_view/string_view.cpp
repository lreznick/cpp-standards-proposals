#include <string>
#include <iostream>
#include <utility>
#include <algorithm>
#include <iterator>
#include <type_traits>
#include <string.h>

struct Verify
{
	~Verify() { std::cout << "Killing lifetime extension" << std::endl; }
};

namespace MyStd
{
	template< typename T, typename = void >
	struct is_temp_string_convertible : std::false_type {};

	template< typename T >
	struct is_temp_string_convertible< T, std::void_t< decltype( std::declval< T >().operator std::string () ) > >
			: std::true_type {};

	template< typename T >
	struct is_temp_string_convertible< T, std::void_t< decltype( std::as_const( std::declval< T >() ).operator std::string () ) > >
			: std::true_type {};

	//template< typename T >
	//struct is_temp_string_convertible< T, std::void_t< decltype( &T const ::operator std::string () ) > >
			//: std::true_type {};

	template< typename CharT >
	class BasicStringView
	{
		private:
			const CharT *first;
			const CharT *last;

			template< typename C >
			friend std::ostream &operator << ( std::ostream &, const BasicStringView< C > & );

		public:
			BasicStringView( const char *s ) : first( s ), last( s + strlen( s ) ) {}
			BasicStringView( const std::string &s ) : first( &s[ 0 ] ), last( &s[ 0 ] + s.size() ) {}

			#if 0
			struct from_string {};

			template< typename AlienType >
			explicit
			BasicStringView( const AlienType &a, from_string )
					: BasicStringView( static_cast< const std::string & >( a ) ) {}

			struct from_cstring {};
			template< typename AlienType >
			explicit
			BasicStringView( const AlienType &a, from_cstring )
					: BasicStringView( static_cast< const char * >( a ) ) {}

			template
			<
				typename AlienType,
				typename= typename std::enable_if<
						std::is_convertible< AlienType, const char * >::value
						|| std::is_convertible< AlienType, const std::string & >::value, void >::type,
				typename tag= typename std::conditional< std::is_convertible< AlienType, const char * >::value,
						from_cstring, from_string >::type
			>
			BasicStringView( const AlienType &a )
					: BasicStringView( a, tag{} ) {}
			#endif

			struct facepunch { friend BasicStringView; private: facepunch()= default; };

			template
			<
				typename AlienType,
				typename= typename std::enable_if
				<
						std::is_convertible< AlienType, const char * >::value
						|| std::is_convertible< AlienType, std::string >::value
						|| std::is_convertible< AlienType, const std::string & >::value
				, void >::type//,
				//typename force_type= typename std::conditional< std::is_convertible< AlienType, const char * >::value,
						//const char *, std::string >::type
			>
			BasicStringView( const AlienType &a, facepunch= {}, std::string &&cheat= {}, Verify = {} )
					: BasicStringView( [&cheat, &a] () -> BasicStringView
					{
						if constexpr( is_temp_string_convertible< AlienType >::value )
						{
							std::cout << "Extending lifetime" << std::endl;
							cheat= a.operator std::string();
							return cheat;
						}
						else if constexpr( std::is_convertible< AlienType, const char * >::value )
						{
							return static_cast< const char * >( a );
						}
						else
						{
							return static_cast< const std::string & >( a );
						}
					}() ) {}


			const char *begin() const { return first; }
			const char *end() const { return last; }
	};
	using StringView= BasicStringView< char >;

	template< typename CharT >
	std::ostream &
	operator << ( std::ostream &os, const BasicStringView< CharT > &v )
	{
		using std::begin; using std::end;
		std::copy( begin( v ), end( v ), std::ostream_iterator< CharT >( os ) );
		return os;
	}
}

namespace MyLib
{
	class MyObject0
	{
		private:
			std::string s= "Hello";

		public:
			operator const std::string &() const { return s; }

			operator const char *() const { return s.c_str(); }
	};

	class MyObject1
	{
		private:
			std::string s= "Hello world";

		public:
			operator std::string () const { std::cout << "Needs extension" << std::endl; return s; }
	};

	class MyObject2
	{
		private:
			std::string s= "Hello world";

		public:
			operator const char *() const { return s.c_str(); }
	};

	class MyObject3
	{
		private:
			std::string s= "Hello world";

		public:
			operator const std::string &() const { return s; }
	};


	class MyObject4
	{
		private:
			std::string s= "Hello world";

		public:
			operator const std::string &() const { return s; }
			operator MyStd::StringView () const { std::cout << "Best choice!" << std::endl; return s; }
	};

	class Example
	{
		public:
			operator std::string () const { return "Hello"; }
	};
}

void
f( MyStd::StringView v )
{
	std::cout << "F Called with: " << v << "..." << std::endl;
}

int
main()
{
	MyLib::MyObject0 o0;
	MyLib::MyObject1 o1;
	MyLib::MyObject2 o2;
	MyLib::MyObject3 o3;
	MyLib::MyObject4 o4;
	MyLib::Example e;
	f( o0 );
	f( o1 );
	f( o2 );
	f( o3 );
	f( o4 );
	f( e );


	{
		MyStd::StringView sanity= "Check 1";
		std::cout << "Checked" << std::endl;
	}

	{
		std::string s= "Check 2";
		MyStd::StringView sanity= s;
		std::cout << "Checked 2" << std::endl;
	}

	{
		std::cout << "Check 3 -- this should be a problem..." << std::endl;
		MyStd::StringView sanity= o1;
		std::string evil= "Corrupt!!!"; // Corrupts buffer that was recently freed, I hope!
		std::cout << "[31mExpect corruption: " << sanity << "[37m" << std::endl;
		std::cout << "Checked 3" << std::endl;
	}

	{
		std::cout << "Check 4 -- this should be a problem..." << std::endl;
		MyStd::StringView sanity= o4;
		std::cout << "Checked 4" << std::endl;
	}

	MyStd::StringView v( o3, MyStd::StringView::facepunch{} );
}
