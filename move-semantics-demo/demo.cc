#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <cstring>
#include <type_traits>

namespace
{
	namespace C
	{
		constexpr bool enableFailingCases= false;
		constexpr bool disableAllCases= false;
	}

	struct LegacyCopyable
	{
		char *data= nullptr;
		std::size_t amount= 0;

		~LegacyCopyable()
		{
			delete data;
		}

		LegacyCopyable( const LegacyCopyable &copy )
			: data( new char[ copy.amount ] ), amount( copy.amount )
		{
			if( !this->amount ) return;
			memcpy( this->data, copy.data, this->amount );
		}

		LegacyCopyable &
		operator= ( const LegacyCopyable &copy )
		{
			LegacyCopyable tmp= copy;

			using std::swap;
			swap( this->data, tmp.data );
			swap( this->amount, tmp.amount );

			return *this;
		}

		// Legacy classes probably have default constructors that use `{}`, not `= default;`
		LegacyCopyable() {}
	};

	struct ImplicitlyMoveOnlyWithCopyable
	{
		std::unique_ptr< std::string > moveOnlyPart;
		std::string copyablePart;
	};

	struct ImplicitlyMoveOnlyWithoutCopyable
	{
		std::unique_ptr< std::string > moveOnlyPart;
	};

	struct ImplicitlyMoveOnlyWithLegacyCopyable
	{
		std::unique_ptr< std::string > moveOnlyPart;
		std::string copyablePart;
		LegacyCopyable legacy;
	};

	struct ExplicitlyMoveableWithCopyable
	{
		std::unique_ptr< std::string > moveablePart;
		std::string copyablePart;

		ExplicitlyMoveableWithCopyable()= default;
		ExplicitlyMoveableWithCopyable( ExplicitlyMoveableWithCopyable && )= default;
		ExplicitlyMoveableWithCopyable &operator= ( ExplicitlyMoveableWithCopyable && )= default;
	};

	struct ExplicitlyMoveableWithoutCopyable
	{
		std::unique_ptr< std::string > moveablePart;

		ExplicitlyMoveableWithoutCopyable()= default;
		ExplicitlyMoveableWithoutCopyable( ExplicitlyMoveableWithoutCopyable && )= default;
		ExplicitlyMoveableWithoutCopyable &operator= ( ExplicitlyMoveableWithoutCopyable && )= default;
	};

	struct ExplicitlyMoveableWithLegacyCopyable
	{
		std::unique_ptr< std::string > moveablePart;
		std::string copyablePart;
		LegacyCopyable legacy;

		ExplicitlyMoveableWithLegacyCopyable()= default;
		ExplicitlyMoveableWithLegacyCopyable( ExplicitlyMoveableWithLegacyCopyable && )= default;
		ExplicitlyMoveableWithLegacyCopyable &operator= ( ExplicitlyMoveableWithLegacyCopyable && )= default;
	};

	struct ExplicitlyCopyableWithCopyable
	{
		std::unique_ptr< std::string > moveablePart;
		std::string copyablePart;

		ExplicitlyCopyableWithCopyable()= default;
		ExplicitlyCopyableWithCopyable( const ExplicitlyCopyableWithCopyable & )= default;
		ExplicitlyCopyableWithCopyable &operator= ( const ExplicitlyCopyableWithCopyable & )= default;
	};

	struct ExplicitlyCopyableWithoutCopyable
	{
		std::unique_ptr< std::string > moveablePart;

		ExplicitlyCopyableWithoutCopyable()= default;
		ExplicitlyCopyableWithoutCopyable( const ExplicitlyCopyableWithoutCopyable & )= default;
		ExplicitlyCopyableWithoutCopyable &operator= ( const ExplicitlyCopyableWithoutCopyable & )= default;
	};

	struct ExplicitlyCopyableWithLegacyCopyable
	{
		std::unique_ptr< std::string > moveablePart;
		std::string copyablePart;
		LegacyCopyable legacy;

		ExplicitlyCopyableWithLegacyCopyable()= default;
		ExplicitlyCopyableWithLegacyCopyable( const ExplicitlyCopyableWithLegacyCopyable & )= default;
		ExplicitlyCopyableWithLegacyCopyable &operator= ( const ExplicitlyCopyableWithLegacyCopyable & )= default;
	};

	struct ExplicitlyCopyableAndMoveableWithCopyable
	{
		std::unique_ptr< std::string > moveablePart;
		std::string copyablePart;

		ExplicitlyCopyableAndMoveableWithCopyable()= default;
		ExplicitlyCopyableAndMoveableWithCopyable( ExplicitlyCopyableAndMoveableWithCopyable && )= default;
		ExplicitlyCopyableAndMoveableWithCopyable &operator= ( ExplicitlyCopyableAndMoveableWithCopyable && )= default;
		ExplicitlyCopyableAndMoveableWithCopyable( const ExplicitlyCopyableAndMoveableWithCopyable & )= default;
		ExplicitlyCopyableAndMoveableWithCopyable &operator= ( const ExplicitlyCopyableAndMoveableWithCopyable & )= default;
	};

	struct ExplicitlyCopyableAndMoveableWithoutCopyable
	{
		std::unique_ptr< std::string > moveablePart;

		ExplicitlyCopyableAndMoveableWithoutCopyable()= default;
		ExplicitlyCopyableAndMoveableWithoutCopyable( ExplicitlyCopyableAndMoveableWithoutCopyable && )= default;
		ExplicitlyCopyableAndMoveableWithoutCopyable &operator= ( ExplicitlyCopyableAndMoveableWithoutCopyable && )= default;
		ExplicitlyCopyableAndMoveableWithoutCopyable( const ExplicitlyCopyableAndMoveableWithoutCopyable & )= default;
		ExplicitlyCopyableAndMoveableWithoutCopyable &operator= ( const ExplicitlyCopyableAndMoveableWithoutCopyable & )= default;
	};

	struct ExplicitlyCopyableAndMoveableWithLegacyCopyable
	{
		std::unique_ptr< std::string > moveablePart;
		std::string copyablePart;
		LegacyCopyable legacy;

		ExplicitlyCopyableAndMoveableWithLegacyCopyable()= default;
		ExplicitlyCopyableAndMoveableWithLegacyCopyable( ExplicitlyCopyableAndMoveableWithLegacyCopyable && )= default;
		ExplicitlyCopyableAndMoveableWithLegacyCopyable &operator= ( ExplicitlyCopyableAndMoveableWithLegacyCopyable && )= default;
		ExplicitlyCopyableAndMoveableWithLegacyCopyable( const ExplicitlyCopyableAndMoveableWithLegacyCopyable & )= default;
		ExplicitlyCopyableAndMoveableWithLegacyCopyable &operator= ( const ExplicitlyCopyableAndMoveableWithLegacyCopyable & )= default;
	};
}

template< typename Element >
static void
testType()
{
	std::vector< Element > vector;

	constexpr static bool canWork= std::is_copy_constructible< Element >()
				&& std::is_move_constructible< Element >();

	for( int i= 0; i < 100; ++i )
	{
		if constexpr( !C::disableAllCases && ( canWork || C::enableFailingCases ) )
		{
			vector.push_back( Element() );
		}
	}

	if constexpr( !C::disableAllCases && ( canWork || !C::enableFailingCases ) )
	{
		vector.reserve( vector.capacity() * 2 );
	}

	std::cout << "It is " << ( std::is_copy_constructible< Element >() ? "" : "not " ) << "copyable and ";

	std::cout << "is " << ( std::is_move_constructible< Element >() ? "" : "not " ) << "moveable: ";
	std::cout << typeid( Element ).name() << std::endl;
}


namespace
{
	struct Experiment
	{
		std::string s;

		Experiment()= default;
		Experiment( const Experiment & )= default;
		Experiment &operator= ( const Experiment & )= default;
	};
}

int
main()
{
	testType< LegacyCopyable >();

	testType< ImplicitlyMoveOnlyWithCopyable >();
	testType< ImplicitlyMoveOnlyWithoutCopyable >();
	testType< ImplicitlyMoveOnlyWithLegacyCopyable >();

	testType< ExplicitlyMoveableWithCopyable >();
	testType< ExplicitlyMoveableWithoutCopyable >();
	testType< ExplicitlyMoveableWithLegacyCopyable >();

	if constexpr( C::enableFailingCases )
	{
		testType< ExplicitlyCopyableWithCopyable >();
		testType< ExplicitlyCopyableWithoutCopyable >();
		testType< ExplicitlyCopyableWithLegacyCopyable >();
	}

	testType< ExplicitlyCopyableAndMoveableWithCopyable >();
	testType< ExplicitlyCopyableAndMoveableWithoutCopyable >();
	testType< ExplicitlyCopyableAndMoveableWithLegacyCopyable >();

	testType< Experiment >();
}
