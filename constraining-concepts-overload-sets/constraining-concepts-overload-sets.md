Constraining Concepts Overload Sets
===================================

ISO/IEC JTC1 SC22 WG21 P0782D2

    ADAM David Alan Martin  (adam@recursive.engineer)
    Erich Keane             (erich.keane@intel.com)
    Sean R. Spillane        (sean@spillane.us)

Abstract
--------

The central purpose of Concepts is to simplify generic programming such that it is approachable to the
non-expert developer.  In general it makes great strides towards this end particularly in the capacity
of invoking a generic function; however, the Concepts design does not deliver on this promise in the
implementation of a generic function.  This is because the feature does not constrain the overload set
of a template-concept function itself.  This is contrary to the expectations of non-experts, because
to them Concepts should strongly resemble the callable properties of an interface.  This mental model
drives their expectations to believe that Concepts offer a mechanism to limit the set of operations
which would be visible from within their constrained function to those which are specified by concept
used by the constrained function.

The fact that this is not the case in constrained functions can lead to surprising violations of
the author's expectations thereof.  Unfortunately, this oversight cannot be corrected later.  To correct
this later would entail silent behavioral changes to existing code after the release of Concepts in a
standard.  In other words, this is our only chance to get this right.

First, we'll outline what the committee asked for in improvements and discuss how we might effect those
results.  Then, we'll explain our much-revised proposal and how it, we believe, solves the problem.
Later we'll discuss the original proposal from P0782R1, leaving it mostly as it was, including motivations
and examples, but correcting some errors.  Finally, we'll anticipate common questions and answer them.

Overload Resolution is Insufficient
-----------------------------------

In Rapperswil, 2018, P0782R1 was discussed, in EWG.  Our simple example regarding a potentially unanticipated
overload to a function found by ADL was discussed in detail.  As discussed in earlier papers and meetings, our
problem is not with ADL per-se, but rather with the fact that "constrained functions" are only constrained
in the sense that callers cannot inappropriately call them, but not that these functions are actually
constrained in what they may call.  Our proposal attempted to use modifications to the overload resolution
rules to achieve these ends.  Discussion in committee revealed this to be insufficient in a number of cases.

A simplification of our original example code (without the complete context) is:

```
namespace AlgorithmLibrary
{
    template< ConceptLibrary::Stringable S >
    void
    fire( const S &s )
    {
        std::cout << s.toString() << std::endl;
    }

    template< ConceptLibrary::Stringable S >
    void
    printAll( const std::vector< S > &container )
    {
        // Our original proposal would (with certain assumptions made about the meaning of
        // `const S &s: container`) not call any free-function `fire` on `s` which was found
        // by ADL in the namespace for `decltype( s )`, because it wasn't used in the concept
        // definition.
        for( const S &s: container ) fire( s );
    }
}
```

The problematic cases discussed in committee can be boiled down to three variations of the core
of the `printAll` function:


__Variation 1__:

```
    for( const S &s: container ) fire( std::identity( s ) );
```

__Variation 2__:
```
    for( const S &element: container )
    {
        const auto &s= element;
        fire( s );
    }
```

__Variation 3__:
```
    std::for_each( begin( container ), end( container ),
            []( const auto &s ){ fire( s ); } );
```

The problems in our simple overload resolution rules that were exposed by these three refactorings of what
*should* be equivalent code require that a solution to this problem be much broader than just overload
resolution.  Each of these three variations appear to be the simplest case of the three shortcomings of
just modifying overload resolution rules:

__Shortcoming 1__: The concept information does not propagate through composition of function calls, such as
`f( g( x ) )`.  The `std::identity` function, in the place of `g` clearly highlights this.  It would likely
be surprising to a programmer if `f( std::identity( x ) )` were completely different to `f( x )`.  In fact,
it would probably cause `std::identity` to be far less useful than it would be for many circumstances.  This
transitivity of Concept concerns is present for more than just `std::identity`, but it concisely illustrates
the case.  In our original proposal, the actual concept information about `decltype( x )` would be lost upon
evaluation of the expression `std::identity( x )`.  Although the `decltype` is the same, the concept
information is lost at compiletime; somehow we would have to revive this constraint information..

__Shortcoming 2__: The `auto` problem.  The `auto` keyword performs type deduction but it doesn't necessarily
propagate the concept information that is available at compiletime into the resulting variable.  This probably
is simple to remedy, but a simple remedy to this does not account for the use of `auto` in the case of
a polymorphic lambda.  The problem is that the concept information associated with an expression (in this case
just a constrained variable) does not necessarily create a likewise constrained variable in the result.  This
would create another surprising case where a temporary reference to a value which is constrained by a Concept
has lost its constraint.  A call to `f( localReference )` would not match the results of a call to
`f( constrainedVariable )` when `auto &localReference= constrainedVariable;`.  This would be equally surprising
in the language, we feel.  Somehow we have to keep this constraint information alive further.

__Shortcoming 3__: The deep-template-expansion problem.  The use of a polymorphic lambda in a call to an
STL algorithm will be unable to access the constraint information on its parameter, as the function template
which calls it is not constrained by any of the concept information known to the caller.  How do we propagate
the constraint information down into the polymorphic lambda for it to use?  The two typical alternatives,
build it into the type system, or build a wrapper object for the constrained value are both equally painful
and unsuitable to use in C++.  Yet the act of refactoring a loop to use a polymorphic lambda and an STL
algorithm shouldn't potentially cause a major alteration in the semantics of that code.  Somehow we have to
reintroduce this constraint information further down the call stack.

All three of these shortcomings are interrelated, and the problem requires an integrated solution; however,
the shortcomings exposed by each of the variants are different aspects of the same problem -- preserving
concept information in C++ for use in later phases of assigning semantics to expression evaluation.

The Problem of Preserving Concept Metadata at Compiletime
---------------------------------------------------------

As discussed above, clearly we require a way to preserve the concept information known at compiletime for
use at various parts of the compilation that are unrelated to the immediate call where overload resolution
rules performed the right selection.

A common solution employed in many languages would be to introduce some kind of wrapper object which "boxes"
up a constrained value into a distinct type and preserves this information in the type system by letting the
box it rides in inform semantics.  This also has the advantage of letting this box act as the discriminator
to control which overloads of a function or operator actually get called.  In some languages this box might
be type-erased as to its pure contents, becoming essentially a C++ class with virtual functions.  This is
probably inappropriate for use in C++ as it sacrifices all knowledge of the underlying type at compiletime,
thus preventing myriad optimizations.

Alternatively the boxing type could be a distinct wrapper for each underlying type, preserving the true type
information throughout the compile process.  The downsides of this are well known.  This is what concept maps
were in C++0x.  We reiterate that we are NOT proposing these.  We merely mention them here to explore the
possibilities and to remind the reader that we've avoided this route.

A more subtle solution would use the type system a bit differently.  Instead of building the concept information
into the type being constrained when passing it down to lower function templates, we silently pass it down
into the lower function's instantiation for use therein, and then we mangle the names of those function
templates in order to encode the fact that that implementation may differ from any other differently
constrained instantiation or an unconstrained instantiation.  This seems tenable at first glance, but it tends
to have some very surprising effects.  Consider the following function template:
`void add_last_one( T, U );`.  Although we wouldn't be making `T` or `U` into a box, both `T` and `U` were
`std::list< ... >` and we passed a `U` which was a constrained `std::list< ... >`, wherein, perhaps, the
`.back` operation is const-only, we start to get into some very problematic territory.  The problem is when
the `u.back()` operation runs, it will be a different one than `T` gets.  The fact that one is `const`
may affect the choice of performing move semantics, which may pessimize a function at best, or even give
different behavior at worst.  Now imagine that this function `add_last_one` is a called by a member function
of a templated container.  Now that container's entire operation varies based upon constraints to its member
functions.  Eventually we wind up with multiple implementations of critical member functions which differ
in the operations they do, which could seriously jeopardize the integrity of the memory layout.  It's almost
as-if two different type implementations were fighting over the same memory.  This mightn't be a problem
if memory layout were identical under all variations, but note that `std::vector< bool >` is a totally
different layout to `std::vector< struct_containing_a_single_bool >`.  Thus we have sort of re-invented the
concept map, just raised it up to a higher level -- push the concept map out to the broadest instantiated type
using it.  This also eventually means that deep copy-in and deep copy-out semantics might become necessary as
the instantiated outer types don't agree on layout, and forcing them to agree can.

Maybe it mightn't be so bad if we could limit the depth of constraint propagation to only a single, or a few
levels.  However, the same ugly problems of ODR violations and layout battles still await us here -- a
function instantiated at "limit - 1" could have different behavior than "limit - 2", as the function at "limit"
now would pass on constraint information one more level and thus change definitions again.  Requiring the
author to know how deep in a call hierarchy of templates a function is instantiation is a completely unviable
option.  And trying to make the compiler solve for some stable expansion which doesn't change meaning runs
into the halting problem.

All of these problems arise from putting constraint information into the type system.  The problem is that
putting things into the type system is kind of an all-or-nothing proposition -- either information is
completely embedded therein, or it is completely absent.

Maybe the type system is the wrong place to put this kind of compiletime metadata.


Keep It Out of the Type System!
-------------------------------

What if there were a way to make this kind of interface-like behavior work without touching the type system?
What kinds of compromises might we have to make?  If it's not in the type system, then where do we put it?

Obviously, the benefit of keeping it out of the type system is to avoid the problems that we outlined in
our section "The Problem of Preserving Concept Metadata at Compiletime".  However, the problem goes deeper
than just keeping it out of the type system -- we can't preserve any of it.  If we preserved constraint
information into deeper template instantiations we'd merely reintroduce the divergent implementations problem
(ODR issue) and then either try to solve it with name mangling (basically the type system) or by just
hoping that it's not a big problem (which it certainly will be a big problem for someone!)

Of course losing the concept information is what we were trying to prevent, so how do we preserve type
information while not preserving it?  We don't!  We instead rely upon reintroducing it at various points
in expression checking.  Our solution relies upon introducing a few new keywords to the language, with
some mostly heretofore-unseen semantics and a few implicit applications of those new grammatical productions.

The end result, we believe, is a reasonable compromise position.  The resulting syntax for users wishing
to preserve constraint information will be nearly the same, if not the same, in most cases.

Before explaining the syntax and introducing the new keywords, we'd like to show the 3 variations of the
problem again and contrast them with the modified form in our expanded proposal:

__Variation 1__:

```
    // The problematic variation:
    for( const S &s: container ) fire( std::identity( s ) );

    // Which under our expanded proposal needn't change at all!
    for( const S &s: container ) fire( std::identity( s ) );

```

__Variation 2__:
```
    // The problematic variation:
    for( const S &element: container )
    {
        const auto &s= element;
        fire( s );
    }

    // Which under our expanded proposal might not need to change, depending upon some direction
    // but in the worst case would become:
    for( const S &element: container )
    {
        const __new_keyword__ &s= element;
        fire( s );
    }
```

__Variation 3__:
```
    // The problematic variation:
    std::for_each( begin( container ), end( container ),
            []( const auto &s ){ fire( s ); } );

    // This kind of rewrite would be possible under our solution (which was also a requirement of the
    // earlier solution):
    std::for_each( begin( container ), end( container ),
            []( const Concept &s ){ fire( s ); } );

    // Or, another possible rewrite:
    std::for_each( begin( container ), end( container ),
            []( const __new_keyword_like_decltype__( container.front() ) &s ){ fire( s ); } );
```


The solution to the third shortcoming is the most disappointing, as preserving the concept through
`std::for_each` for consumption by `auto` was strongly desired by both the committee and us.  We will justify
in our conclusions why we're not as disappointed as we might have been, however.

How to Reinvent Lost Concept Constraints
----------------------------------------

In order to make it easier to understand this new battery of features, we will introduce them in a specific
order, and build each from earlier primitives, using "as-if" and "almost as-if" explanations of their meanings.

We will start with a new kind of cast, `concept_cast`.  It should be, hopefully, fairly self-evident what
it does, but this is how we define it:

```
concept_


Simple Motivating Example
-------------------------

In our original paper, this motivating example was to highlight some potentially unexpected problems with
ambiguity.  After discussion in Rapperswil, the authors are content to accept that the
`LoanInterestSerializer` in the following example should probably be considered a poorly written class.
The major change to the overload resolution rule that this relaxation makes is to consider the entire viable
set, not just the best match, in determining what overloads in a type which models a concept are suitable
overload candidates for a specific call site.  This has the advantage of mostly solving the "implicit
expression variants" problem, such as r-value vs. l-value overloads.

```
// Assume a concept, `IntegerSerializer` which requires a member function called `serialize` which
// takes an `int` and returns a `std::string` with the representation of that integer, in the format
// appropriate to the implementation.

template< typename Instance >
concept IntegerSerializer= requires( Instance instance, int value )
{
    { std::as_const( instance ).serialize( value ) } -> std::string;
}


class SimpleSerializer
{
    std::string serialize( int value ) const;
};

struct PrecisionSerializer
{
    std::string serialize( double preciseValue ) const;
};

struct LoanInterestSerializer
{
    std::string serialize( int interestBasisPoints ) const;
    std::string serialize( double interestRate ) const;
};

// This is a terse-syntax style template function.  Whether the author knew he or
// she was writing a template is not necessarily clear.
std::string formatLogarithmicValue( IntegerSerializer &serializer, int integerValue )
{
    return serializer.serialize( std::log( integerValue ) );
}
```

The issue is in what kind of object we pass to `formatLogarithmicValue`.  If we pass an `SimpleSerializer`,
then no surprises happen -- the `double` is implicitly converted to `int`.  If we pass `PrecisionSerializer`,
then the definition of the concept will pass and an implicit conversion to `int` will happen, which is
potentially surprising; however, many programmers are reasonably comfortable with the idea of fundamental
type conversions.  The most surprising case is that of `LoanInterestSerializer`.  `LoanInterestSerializer`
provides a `double` and an `int` overload.  Although the concept requested function with a signature that
accepts an `int`, the overload which accepts `double` in its signature will be called.

From the perspective of the compiler, the way it will actually compile the function `formatLogarithmicValue`
is as if it were written:

```
template< typename IntegerSerializer >
std::string
formatLogarithmicValue( IntegerSerializer &serializer, int integerValue )
{
    return serializer.serialize( std::log( integerValue ) );
}
```

At this point, the invocation of `IntegerSerializer::serializer` will be whatever best matches
`decltype( std::log( integerValue ) )`, which is the overload with `double` as its parameter.
This is likely surprising behavior to the author of `formatLogarithmicValue`, as well as the caller
of `formatLogarithmicValue`.  Both of these authors would expect that the constraints described by
the concept would be obeyed, yet paradoxically the overload which was not the best match for the
constraint was actually the overload that was actually invoked in the body of the "constrained"
function!

The only way for an author of such a constrained function to avoid this, at present, is to rewrite
`formatLogarithmicValue` in such a way as to prevent the incorrect lookup.  Unfortunately, this requires
a level of C++  expertise regarding name lookup and overload resolution which is at odds with the level
of expertise expected of the audience of Concepts, viz. the non-expert programmer.  Such a rewrite
might appear thus:

```
template< typename IS >
std::string
formatLogarithmicValue( IS &serializer, int integerValue )
        requires( IntegerSerializer< IS > )
{
    return std::as_const( serializer ).serialize(
            static_cast< int >( std::log( static_cast< double >( integerValue ) ) ) );
}
```

This does not appear to be code that would be expected of the audience targetted by Concepts.  Additionally,
although one of the authors of this paper is author of `std::as_const`, this is not the purpose nor audience
he had in mind when he proposed it.

The primary purpose of the static casts and as-consts in this rewrite are actually dedicated to the selection
of the correct overload, not to any specific need to have a value in the form of any specific type.

An Example at Scale
-------------------

In this example, distinct namespaces are used to highlight the fact that different pieces of code were written
by different authors, in different code bases, perhaps at different times.  The `UserProgram` namespace is
representative of a final application author trying to tie together disparate libraries in C++ and produce
a coherent, correct result from a program.


```
namespace ConceptLibrary
{
    template< typename Instance >
    concept Stringable= requires( Instance instance )
    {
        { std::as_const( instance ).toString() } -> std::string;
    }
}

namespace PayrollLibrary
{
    class Employee
    {
        private:
            std::string name;

        public:
            Employee( std::string initialName );

            // This satisfies the Stringable concept, and the author of this type knows that.
            std::string toString() const;

            friend bool operator == ( const Employee &lhs, const Employee &rhs );

            // The following functions are friend, to indicate that ADL is intended.

            // Terminate the specified employee's employment
            friend void fire( const Employee &emp );

            // Initiate the specified employee's employment
            friend void hire( const Employee &emp );

            // Returns true if the specified employee is employed and false otherwise.
            friend bool worksHere( const Employee &emp );
    };
}

namespace AlgorithmLibrary
{
    // This "fires" off a stringable object to be processed.  It is a terse-syntax style
    // template function.  Whether the author knew he or she was writing a template is
    // not necessarily clear.  Although it is likely that the author knew the function
    // was a generic function.
    void
    fire( const ConceptLibrary::Stringable &s )
    {
        std::cout << "I am interested in " << s.toString() << std::endl;
    }

    // This too is a terse-syntax style template function.  Whether the author knew he or
    // she was writing a template is not necessarily clear.  Although it is likely that the
    // author knew the function was a generic function.  The purpose of this function is
    // to call the above helper function in a loop.
    void
    printAll( const std::vector< ConceptLibrary::Stringable > &v )
    {
        for( auto &&s: v ) fire( s );
    }
}

namespace UserProgram
{
    void code()
    {
        std::vector< PayrollLibrary::Employee > team;
        team.emplace_back( "John Doe" );
        AlgorithmLibrary::printAll( team );
    }
}
```

In this example the intent of the three separate authors is apparent.  The author of the `PayrollLibrary`
simply wished to afford his users the ability to represent employees at a company.  The author of the
`AlgorithmLibrary` wished to afford his users the ability to print printable things and needed to write
an internal helper method to better organize his code.  The author of `UserProgram` naievely wished
to combine these reusable components for a simple task.  However, a subtle behavior of name lookup
in function templates resulted in the termination of employees, rather than the intended call to an
implementation detail.  This happened because the name of the implementation detail happened to collide
with the name of some irrelevant API in the `Employee` object being processed.  Recall that the author of
`AlgorithmLibrary` is likely unaware of the fact that types may exist which are `Stringable` and yet
interfere with the name he chose for his internal implementation detail.

The authors of this paper recognize the importance of respecting the original intent of the programmers
of these components without burying them in the details of defensive template writing.  These kinds of
examples will come up frequently and perniciously in codebases which import third party libraries and
work in multiple groups each with different naming conventions.  Even without variance in naming conventions,
names that have multiple meanings to multiple people are likely to be used across disparate parts of
a codebase, and thus they are more likely to exhibit this pathological behavior.


Why it is Important to Address this Now
---------------------------------------

Should the terse syntax be accepted into the current standard, without addressing this issue, then
future attempts to repair this oversight in the language specification, leave us with one of two
incredibly unpallateable alternatives and one unsatisfying one:

 1. Make constraint violations an error, thus requiring extreme verbosity.
 2. Silently change the meaning of existing code, with ODR implications, because these constrained functions
    are templates.
 3. Introduce a new syntax for indicating that a function definition should be processed in a manner which
    is more consistent with average programmer expectations; 

In the first case, vast amounts of code will fail to compile in noisy ways.  After that point, the user code
would need to be rewritten, in a manner similar to the necessary rewrites as described above. 

In the second case, massive fallout from ODR, silent subtle semantic changes, and other unforseen dangers
lie in wait.

In the third case, the benefits of the "natural" syntax are lost, as the best syntax for beginners is no
longer the natural syntax!  This obviously defeats the intended purpose of Concepts with a natural syntax.

Some Design Philosophy
----------------------

There are other cases where current Concepts can fail to prevent an incorrect selection of operation.
This fails to deliver upon a big part of the expected benefits of this language feature.  The comparison has
been drawn between C++ Virtual Functions and Concepts.  As Concepts are being presented to bring generic
programming to the masses, it is vital that 3 core safety requirements be considered.  These requirements
are similar to aspects of Object Oriented Programming.

1. An object passed to a function must meet the qualifications that a concept describes.  This is
   analagous to how a function taking a pointer or reference to a class has the parameter checked for
   substitutiablility.  The current Concepts proposal provides this extremely well.

2. An object written to be used as a model of a concept should have its definition checked for completeness
   by the compiler.  This is analagous to how a class is checked for abstractness vs concreteness.
   The current Concepts proposal lacks this.  However, this is approximated very well by the concept
   checking machinery.  This guarantees that every class which is matched to concept provides a definition
   for every required operation under that concept, thus satisfying the requirements of the concept.

3. A constrained function is only capable of calling the functions on its parameters that are described by
   its constraining Concepts.  This is analagous to how a function taking a pointer to base is only allowed
   to call members of the base -- new APIs added in any derived class are not considered to be better
   matches, ever.  The current Concepts proposal lacks anything resembling this, and this oversight has yet
   to be addressed.  It is this deficiency which our paper seeks to remedy.

We propose that the Concepts feature is incomplete without constrained overload set for usage, thus satisfying
the third requirement of any interface-like abstraction.  It is vital that we explore this issue.

We recognize that some complexity in the space of constrained generics will always be present, but we feel
that it is best to offload this complexity to the author of a concept rather than to the implementor of a
constrained function.  This is because we believe that fewer concept authors will exist than concept "users".
Additionally, the level of expertise of a concept author is inherently higher than the intended audience
of constrained functions.  In the worst case scenario, a naive definition of a concept will merely result
in a few missed opportunities for more suitable overloads to handle move semantics, avoid conversions,
and other shenanigans

What This Paper is  _<b><u>NOT</b></u>_  Proposing
--------------------------------------------------

 1. Any requirement on optimizers to make any aspect of the code generated by this solution more efficient
 2. Definition Checking
 3. C++0x Concept Checking
 4. C++0x Concept Maps
 5. The generation of "invisible" proxy types
 6. The generation of "invisible" inline proxy functions
 7. Dynamic dispatch
 8. Function call tables
 9. Implicit generation of adaptors
10. Any form of extra code generation
11. Any form of extra type generation
12. Encoding concepts into the type system.


Our Proposed Solution
---------------------

We propose a moderate alteration of the overload resolution rules and name lookup rules that statically
filters out some overloads based upon whether those functions are used to satisfy the concept's requirements.
This constrained overload set hides some functions from visibility in the definition of constrained
functions.  No change should be necessary to the rules of name lookup itself; however, our new overload
resolution rules will affect the results overload resolution on unqualified name lookup in constrained
functions-- this is by design.  We also suggest that it might be desirable to borrow a keyword (we suggest
`explicit`) to indicate that this amended lookup rule should be followed.  The need for this keyword to
enable these lookup rules will be discussed further in the "Design Considerations" section of this paper.


Specifically, our design is to change overload resolution to be the following (taken from cppreference.com's
description of the overload resolution rules):

<table border=10>
<tr>
<td>
<h3>Current overload resolution</h3>
<td>
<h3>Desired overload resolution</h3>
<tr>
<td valign=top>
<p>
Given the set of candidate functions, constructed as described above, the next step of overload
resolution is examining arguments and parameters to reduce the set to the set of viable functions
<p>
To be included in the set of viable functions, the candidate function must satisfy the following:
<p>
<ol>
<li> If there are M arguments, the candidate function that has exactly M parameters is viable
<li> If the candidate function has less than M parameters, but has an ellipsis parameter, it is viable.
<li> If the candidate function has more than M parameters and the M+1'st parameter and all parameters
that follow must have default arguments, it is viable. For the rest of overload resolution, the
parameter list is truncated at M.

<li> If the function has an associated constraint, it must be satisfied
    (since C++20)

<li> For every argument there must be at least one implicit conversion sequence that converts it to the
corresponding parameter.
<li> If any parameter has reference type, reference binding is accounted for at this step: if an rvalue
argument corresponds to non-const lvalue reference parameter or an lvalue argument corresponds to rvalue
reference parameter, the function is not viable.
<td valign=top width=50%>
<p>
Given the set of candidate functions, constructed as described above, the next step of overload
resolution is examining arguments and parameters to reduce the set to the set of viable functions

<p>
To be included in the set of viable functions, the candidate function must satisfy the following:
<p>
<ol>
<li> If there are M arguments, the candidate function that has exactly M parameters is viable
<li> If the candidate function has less than M parameters, but has an ellipsis parameter, it is viable.
<li> If the candidate function has more than M parameters and the M+1'st parameter and all parameters
that follow must have default arguments, it is viable. For the rest of overload resolution, the
parameter list is truncated at M.

<li> If the function has an associated constraint, it must be satisfied
    (since C++20)
<p>
<font color=green>
<li face="X"> 
If at least one of the arguments to the function is constrained and that function was found by
unqualified name lookup and the lookup found a name that is otherwise not visible at the calling
location, then the function is only viable if that function was necessary to satisfy the argument's
concept constraint.
    (this paper)
   
</font>

<li> For every argument there must be at least one implicit conversion sequence that converts it to the
corresponding parameter.
<li> If any parameter has reference type, reference binding is accounted for at this step: if an rvalue
argument corresponds to non-const lvalue reference parameter or an lvalue argument corresponds to rvalue
reference parameter, the function is not viable.

</table>

Objections, Questions, and Concerns
-----------------------------------

__Q:__ Isn't this just C++0x Concepts with definition checking all over again?

__A:__ No.  C++0x Concepts used mechanisms and techniques which are drastically different to the solution
   we have proposed.  In the original C++0x Concepts, the compiler was required to create invisible
   wrapping types (Concept Maps) and create inline shimming functions, and to calculate the set of
   archetype types for a Concept.  C++0x Concepts were difficult to inline and relied upon the optimizer
   to eliminate the overhead imposed by these automatically generated constructs.  C++0x concepts also
   provided "definition checking", which was desirable in that a template function could be checked for
   correctness before instantiation.  C++0x concepts used the "Concept Map" and archetype computations
   to decide whether a template would be correct.  This compiletime computation of an archetype runs
   into a manifestation of the halting problem, which makes generalizing this solution to user defined
   concepts a problematic proposition at best.
<br>
<br>
   By contrast, our solution does not involve the generation of such machinery.  We require no generation
   of any adaptors, maps, or proxies.  Instead, we propose altering and refining the lookup rules to further
   obey the restrictions imposed by Concepts in a manner similar to what is already in the existing design.
   We feel that this is appropriate, because Concepts already requires some alteration to the lookup rules,
   and our design appears to be consistent with the general lookup rule restrictions thereby imposed.
   Concept restrictions, in their current form, are enforced in C++ through lookup rules, not through any
   other mechanism. Our solution merely adds more rules to the set of lookup rules employed in concept
   processing.  This also means that our solution is not capable of providing definition checking -- the
   lookup rules that this solution alters are those which occur during the second phase of name lookup,
   after template instantiation.


<br>

__Q:__ Will I be able to call internal helper functions to my constrained function using an unqualified name?

__A:__ Yes.  We place no restrictions on the calling of functions in namespaces that are unrelated to the
   concept used in a constraint.  The namespaces associated with the types that are constrained are also
   still searched, but only the names which are necessary (in some fashion) to meet the requirements of the
   concept are considered to be viable.  In some sense this is the existing Concepts restriction on calling
   a constrained function applied in reverse -- constraints restrict which functions are called based upon
   their arguments.  The current restriction prevents calling a function which is not prepared to accept
   a type.  Our refinement prevents calling a function which is not presented as part of the requirements
   on a type.  This example is illustrated in our "at scale" example, and it is one of our primary
   motivations.

<br>

__Q:__ Isn't your real problem with {ADL, const vs. non-const overloads, overload resolution, dependent lookup,
   etc.} and not with the lookup rules of Concepts today?

__A:__ Absolutely not.  We have examples of unexpected selection of operation each and every one of
   these cases.  We are not convinced that our problem is with every single one of the above aspects of
   the language.  There are some cases which will be redundantly resolved by improving those aspects
   of the language; however, many problem cases within each of these domains still remain.  This is
   especially true of ADL functions.  ADL functions are intended to be part of the interface of a class;
   however, a constrained value is also a constrained interface.

<br>

__Q:__ How do I actually invoke ADL functions that I want invoked in my constrained functions?

__A:__ ADL functions on an object which are actually part of the interface defined by the concept that
   constrains the calling function are still preserved as part of the viable overload set.  This makes
   them likely to be the best match for an unqualified call, unless a more specific overload is available
   in some other reachable namespace.  If you wish to call ADL functions which are not part of the constraint,
   then one always has two viable options.  The first is to use a direct "using std::foobar" approach, and
   the second is to adjust the definition of a Concept to include this ADL operation.

<br>

__Q:__ What about calling efficient `swap` on an `Assignable`?

__A:__ This is actually a special case of the above concern.  In this case, there are at least two viable
       options.  The first is to add `swap( a, b )` to the requirements of the `Assignable` concept.
       The second is to make `std::swap` have an overload which accepts a value which is a model of
       the `Swapable` concept.  An unqualified call to `swap` after the traditional `using std::swap;`
       declaration will invoke that `Swapable` overload, thus giving the correct behavior.  In addition,
       this has the added benefit of making any direct call to `std::swap` in any context always take
       the best overload!  Although this library change is not proposed by this paper, the authors would
       strongly support such a change.

<br>

__Q:__ Is this going to give me better error messages?

__A:__ Although this is highly dependent upon the details of an implementation, it is possible that
       better error messages would be possible under this proposal.  The instantiation of a template
       which requires some specific operation which is not part of a concept should give a better error
       message -- something along the lines of "Function not found during constraint checking."

<br>

__Q:__ What about implicit conversions needed in calling a function which is part of a concept?

__A:__ Because the function selected by overload resolution must be part of the operations necessary to
       satisfy the constraints of the specified concept, all implicit conversions which are necessary to
       invoke that function are also part of the operations necessary to satisfy that concept.  This means
       that any set of implicit conversions provided by a type which are necessary to invoke a selected
       overload should be "whitelisted" for use in constrained functions when calling that overload.

<br>

__Q:__ Will I be able to invoke arbitrary operations on my constrained parameters which are not part of
       the concept?

__A:__ Any function which is available directly in the namespace (directly or via a `using` statement) of a
       constrained function may be called.  Any function which is a template but is not constrained
       will be called as an unconstrained function.  In that context of an unconstrained template, any
       function may be invoked as normal.  We also propose that an intrinsic cast-like operation could
       be added which will revert a constrained variable to an unconstrained variable, to permit calling
       functions in an unconstrained fashion for various purposes.

Design Considerations
---------------------

Any design that proposes to change lookup rules should not invalidate code written under those
rules today.  Because of this, we do not propose that the lookup rules should be changed when
evaluating names within a "classical" template context.  However, we see a number of opportunities
to apply our modified lookup scheme:

### Terse syntax constrained functions

The terse syntax intends to open generic programming to a wider audience, as discussed earlier.
We feel that it is obvious that such terse syntax functions be subject to rules which provide a more
intuitive result.  Therefore we suggest that any terse syntax considered by the committee must have
the intuitive semantics provided by our proposal.

### Template syntax constrained function

<table>
<tr>
<td width=2%>
<td>
<h4>  All constrained function templates</h4>

Any expansion of a terse syntax from the terse form into a "canonical" production of a constrained
template function declaration could automatically have these rules applied.  This seems fairly obvious
in many respects, because the purpose of Concepts is to afford better selection of applicable functions
in name lookup and overload resolution.  When a user writes a constrained function, even using template
syntax, he or she is explicitly choosing to have the semantics of Concepts applied to their function.
Therefore, it seems a reasonable choice to make every constrained function obey these lookup rules.


<h4> Opt-in for these rules as part of the definition of a constrained function template</h4>

A user trying to modernize a code base by adding constraints to existing template functions, may wind
up causing subtle changes in the semantics and or ODR violations.  Additionally, the template expert is
already intimately familiar with the consequences of C++'s uninituitive lookup rules in templates and may
wish to leverage the semantics afforded by these rules in the implementation of his template function --
he only wishes to constrain the callers, but not himself.

Therefore it may be necessary to control the application of this modified rule through the use of
a signifying keyword.  We propose `explicit template< ... >` as this syntax, as it reads reasonably
well and clearly indicates intent.  Although this proposed syntax uses the `template` keyword, which is
already indicitive of potential lookup dangers, it eschews the pitfalls for the `template< ... >` case.
</table>

Regardless of whether the new lookup rules are opt-in or opt-out, the language loses no expressivity.
It is possible to choose the opposite alternative through other syntax.


### Behavior of These Rules Under Short Circuit of Disjunction

We preserve the viability of overloads which are found on either branch of a disjunction, because
a user would reasonably expect these overloads to be available if those constraints are satisfied.
For branches of a disjunction which are not satisfied, those overloads will be unavailable, as 
the constraint wasn't satisfied.  This seems to result in a viable overload set which most closely
conforms to user expectations.  The overall compile-time cost of this added checking should be
proportional to the overall cost of treating a disjunction as a conjunction for this feature.

Conclusion
----------

In P0726R0 it was asked if Concepts improve upon the expressivity of C++17.  The response from EWG was
mixed.  The fact is that, although Concepts are more approachable and readable than C++17 `std::enable_if`
predicates, they do not provide any new expressivity to the language, nor do they provide any facility
for making templates actually easier to write.  We feel that this proposal's adoption will give a new
dimension to the Concepts feature which will enable the simple expression of C++ generic code in a much
safer and more readable style than C++17 or the current Concepts design.


Revision History
----------------

P0782R0 - A Case for Simplifying/Improving Natural Syntax Concepts (Erich Keane, A.D.A. Martin, Allan Deutsch)

The original draft explained the general problem with a simple example.  The motivation was to help attain
a better consensus for "Natural Syntax" Concepts.  It was presented in the Monday Evening session of EWG at
Jacksonville, on 2018-03-12.  The guidance from the group was strongly positive:
<p>
SF: 10 - F: 21 - N: 22 - A: 7 - SA: 1

P0782R1 - Constraining Concepts Overload Sets (A.D.A. Martin, Erich Keane)

The revised paper explains a solution to the specific problem of getting expected overload resolution out of an
individual function call in a constrained context.  It was presented to EWG during a day session at
Rapperswil, in 2018.  The guidance from the group was strongly positive to continue to pursue work in this
direction:
<p>
SF: 20 - F: 11 - N: 8 - A: 1 - SA: 1
<p>
The group also requested and encouraged investigation into a few things:
<p>
Make concepts "viral" through function calls into templates and in `auto` such that some problematic
examples are corrected:
<p>
SF: 10 - F: 10 - N: 8 - A: 1 - SA: 2
<p>
Make the overload resolution rule also apply to a fully qualified name, not just members and unqualified
names:
<p>
SF: 11 - F: 12 - N: 6 - A: 0 - SA: 0



Acknowledgements
----------------

The authors would like to thank Allan Deutsch, Hal Finkel, Lisa Lippincott, Gabriel Dos Reis, Justin McHugh,
Herb Sutter, Faisal Vali, and numerous others for their research, support, input, review, and guidance
throughout the lifetime of this proposal.  Without their assistance this would not have been possible.

References
----------

P0726R0 - "Does Concepts Improve on C++17?"
P1079R0 - "A minimal solution to the concepts syntax problems"
P0745R1 - "Concepts in-place syntax"
P1013R0 - "Explicit concept expressions"
P1086R0 - "Natural Syntax: Keep It Simple"
P1084R0 - "Today's return-type-requirements Are Insufficient"
