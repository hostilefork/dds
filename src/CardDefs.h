//
// CardDefs.h
//
// These are first steps at proposed classes to improve the type-correctness
// of the DDS solver.  The aim is to catch bugs and increase code clarity,
// while giving the same performance (in non-debug, optimized builds).  The
// methodologies used here are for the C++11 standard of the compiler, which
// was already in use by much of the industry as C++0x before 2011, and is
// now considered "C++" by most programmers who practice the language.
//
// Terminology assistance for naming from:
//
//   http://en.wikipedia.org/wiki/Glossary_of_contract_bridge_terms
//
// Proposal submitted by brian@hostilefork.com, circa 20-Nov-2014
//

#ifndef DDS_CARDDEFS_H_INCLUDED
#define DDS_CARDDEFS_H_INCLUDED

#include <stdexcept>
#include <cassert>
#include <vector>

#include "optional/optional.hpp"


// For testing...
#define DDS_CHECKING_CARDDEFS 1

#if DDS_CHECKING_CARDDEFS
#define DDS_CARDDEFS_CHECKVALID() checkValid()
// Use this when something is only constexpr when not checking
#define DDS_CARDDEFS_CONSTEXPR
#else
// http://stackoverflow.com/a/1306690/211160
#define DDS_CARDDEFS_CHECKVALID() do {} while(0)
#define DDS_CARDDEFS_CONSTEXPR constexpr
#endif



//
// [SpecializeOptional]
//
// Template specialization tool to use with optional from the boost library
// or the pending std::optional abstraction:
//
//   http://www.boost.org/doc/libs/1_57_0/libs/optional/doc/html/index.html
//   http://en.cppreference.com/w/cpp/experimental/optional
//
// The reason for tailoring a class to use in the DDS classes specifically
// is to pay entirely for the added type safety at compile time.  The
// generic implementation of optional<T> templating adds a "tracking boolean"
// to the size of the wrapped type.  This can cost from 1-4 bytes of overhead
// depending on the struct alignment vs. sizeof(T).  For small T (such as a
// byte representing a suit, or a short int representing a set of ranks) this
// cost can add up...especially in arrays.
//
// With specialization, type safety is achieved while having types decide a
// bit pattern to represent the optional case.  That means `optional<Suit>` is
// a distinct type from `Suit`, for instance.  So instead of having strange
// values like "-1" which are not type checked to mean "no card suit"
// (compared to 0=Spade, 1=Heart, 2=Diamond, 3=Clubs), you can let the
// compiler catch differences in expectation...and pass a Suit to a routine
// expecting an optional<Suit> but not vice-versa.
//
// For a blog entry describing the methodology used, see:
//
//   http://blog.hostilefork.com/template-specialize-optional-or-not/
//

template <class T>
class SpecializeOptional : public T {
public:
  SpecializeOptional (T const & value) :
    T (value)
  {
  }

  SpecializeOptional (std::experimental::nullopt_t const &) {
    T::setOptional();
  }

  SpecializeOptional ()
  {
    T::setOptional();
  }

  explicit operator bool() const noexcept {
    return !T::testOptional();
  }

  SpecializeOptional & operator= (std::experimental::nullopt_t const &) {
    T::setOptional();
    return *this;
  }

  bool operator== (SpecializeOptional const & rhs) const {
    if (T::testOptional())
       return !rhs;
    if (rhs)
       return *this == *rhs;
    return false;
  }

  bool operator!= (SpecializeOptional const & rhs) const {
    if (T::testOptional())
      return rhs;
    if (!rhs)
      return true;
    return *this != *this;
  }

  bool operator== (T const & rhs) const {
    if (T::testOptional())
      return false;
    return *this == *rhs;
  }

  bool operator!= (T const & rhs) const {
    if (T::testOptional())
      return true;
    return *this != *rhs;
  }

  friend bool operator==(T const & lhs, SpecializeOptional const & rhs) {
    // just reverse it; this global operator doesn't have friend rights
    // on T.data...
    return rhs == lhs;
  }

  friend bool operator!=(T const & lhs, SpecializeOptional const & rhs) {
    // just reverse it; this global operator doesn't have friend rights
    // on T.data...
    return rhs != lhs;
  }

  T const & operator* () const {
     assert(!T::testOptional());
     return *this;
  }

  T & operator* () {
     assert(!T::testOptional());
     return *this;
  }
};



//
// [Suit] and [Strain]
//
// Two distinct classes to represent either the printed suit of a card, or
// the suit with an option of being NoTrump.  If NoTrump is allowed then
// it is considered a "Strain" instead of a "Suit" and type-checked as
// such.  Though you can assign a Suit to a Strain you cannot do the
// reverse.
//
// In order to interface with (and gradually upgrade) legacy code, a Suit
// is able to coerce to an integer or be constructed from one.  The historical
// integer values in DDS for suits/strains were:
//
//   Spades=0, Hearts=1, Diamonds=2, Clubs=3, NoTrump=4
//
// These values are unfortunate.  An ideal comparison for suits would
// order them in terms of the "stronger" suits being higher.  One might
// argue that the number is rather measure of "weakness"...so you should
// just substitute a less-than operation for the greater-than which
// you might use in card ranks.  Picking 4 for NoTrump throws a wrench
// into that argument.
//
// This did not cause much trouble for DDS because it was largely table-driven
// and as such, would pick answers out of tables instead of looping through
// cards.  Actually, the main place where the comparison operator came into
// play was simply in `for` loops over the suits.  Typical code:
//
//    for (int suit = 0; suit < DDS_SUITS; suit++) { ... }
//
// But this becomes a problem once we start using a type-safe Suit class:
//
//    for (Suit suit (0); suit < DDS_SUITS; suit++) { ... }
//
// Why a problem?  Because the suit class is not willing to hold an
// invalid value of 4...not even long enough to coerce itself into
// that 4 for the comparison of `suit < DDS_SUITS` to fail.  A Suit
// of 4 is simply not valid.
//
// In the interest of interfacing with legacy patterns, I devised a
// workaround that uses instead <= and a special type-safe enum to
// represent the limit.  This gives a unique unchecked case for
// comparing against values one out of bound *without* needing to
// "dereference" the invalid state of 4:
//
//    for (Suit suit (0); suit <= Suit::max(); suit++) { ... }
//
// Really the trick here is that the legacy increment operator does no
// debug checks, so it will let you increment to 4 or 5 or any internal
// state of the Suit that you like.  That would be a problem if you
// ever tried to use the Suit as an integer (or almost anything) but
// because Suit::max() is a special *non-integral* type, the <= operator
// can be specialized to not check.  Never converting the Suit to an
// invalid integer keeps the type safety intact.
//
// If you're wondering about the choice to export the max() value (e.g. 3)
// and use operator<= instead of making max() the first invalid value (e.g. 4)
// and use operator< ... it's because precedent for the meaning of max() is
// set in std::numeric_limits as the "maximum finite value of a given type":
//
//    http://en.cppreference.com/w/cpp/types/numeric_limits
//
// There are costs and benefits to using the meaning.  It's good because it
// is a valid value in the domain.  It's bad because if you're going to
// declare an array indexed by all suits you need to write something like:
//
//    int counts[Suit::max() + 1];
//
// Yet if you consider a ranged type with min() and max() and wanting an
// array covering it, the real optimal way to write that can't just be in
// terms of max() anyway.  To do it right you need:
//
//    int whatever[Foo::max() - Foo::min() + 1];
//
// Some memory was wasted in arrays previously because (for instance) a
// card rank can go to 2 to 14.  There's little point in the first two
// slots of an array indexed by Rank for invalid data.  Given that it is
// complex, it's not such a big problem to deal with the +1 ... as long as
// you only have to write the correct code in one place!
//
// The integer behaviors are more academic than anything...and show the
// mind-bending ability for C++ to let you incrementally upgrade a legacy
// codebase without having to do it all at once.  The fast, modern, flexible
// way uses the range-based FOR:
//
//     for (auto suit : Suit::highToLow()) { ... }
//
// Now it's possible to iterate over any collection of suits in a
// type safe way.  It would actually be possible to make Suit::highToLow
// a std::array as a static member of Suit and so this code would be
// equivalent under-the-hood to:
//
//     unsigned short const highToLow[4] = {3, 2, 1, 0};
//     for (int index = 0; index < 4; index++) {
//        int suit = highToLow[index];
//        ...
//     }
//
// For practical purposes of keeping everything in the header file,
// Suit::highToLow() returns a static const std::vector.
//
// Rather than rewrite the code for Suit and Strain two times, there is a
// base class StrainT which is parameterized with a "noTrumpOk" flag.
// Suit and Strain are just instantiations of this template.
//

template<bool noTrumpOk>
class StrainT {
private:
  unsigned char value;

  template<class T> friend class SpecializeOptional;
  static const unsigned char optionalValue = 0xBD;
  void setOptional() {
     value = optionalValue;
  }
  bool testOptional() const {
     return value == optionalValue;
  }

  enum limitType {
    minValue = 0,
    maxValue = noTrumpOk ? 4 : 3
  };

#if DDS_CHECKING_CARDDEFS
  void checkValid(bool optionalOk = false) const {
    assert(
      ((value != optionalValue) ? true : optionalOk)
      || ((value <= StrainT::max()) && (
        ((value != 4) ? true : noTrumpOk)
      ))
    );
  }
#endif


/// STRAIN: LEGACY DDS 2.x INTEGER INTERFACE

public:
  static constexpr limitType min() { return minValue; }
  static constexpr limitType max() { return maxValue; }

  StrainT (limitType limit) : value(limit) {}

  bool operator<= (limitType limit) const {
    return value <= limit;
  }

  StrainT & operator++ (int) { // http://stackoverflow.com/q/3574831
    value++; return *this;
  }

  StrainT & operator-- (int) { // http://stackoverflow.com/q/3574831
    value--; return *this;
  }

  operator unsigned char() const {
    DDS_CARDDEFS_CHECKVALID();
    return value;
  }


/// STRAIN: MODERN C++11 INTERFACE

public:
  StrainT ()
#if DDS_CHECKING_CARDDEFS
    : value (0xAE) // garbage if checking, don't pay for init if not
#endif
  {
  }

  explicit StrainT (unsigned char value) :
    value (value)
  {
    DDS_CARDDEFS_CHECKVALID();
  }

public:
  bool operator> (StrainT const & rhs) const {
    if (rhs.value == 4) return false;
    return value < rhs.value;
  }

  bool operator>= (StrainT const & rhs) const {
    if (rhs.value == 4) return value == 4;
    return value <= rhs.value;
  }

  bool operator< (StrainT const & rhs) const {
    if (value == 4) return false;
    return value > rhs.value;
  }

  bool operator<= (StrainT const & rhs) const {
    if (value == 4) return rhs.value == 4;
    return value >= rhs.value;
  }

  bool operator== (StrainT const & rhs) const {
    return value == rhs.value;
  }

  bool operator!= (StrainT const & rhs) const {
    return value == rhs.value;
  }

  char toChar() const {
    DDS_CARDDEFS_CHECKVALID();

    static char const strainChars[5] = {
      'S', 'H', 'D', 'C', 'N'
    };
    return strainChars[value];
  }
};


class Strain : public StrainT<true /* NoTrump legal */> {
public:
  using StrainT::StrainT; // inherit all constructors (C++11)

public:
  static std::vector<Strain> const & highToLow() {
    static const std::vector<Strain> highToLowStrain = {
      Strain(4), Strain(0), Strain(1), Strain(2), Strain(3)
    };
    return highToLowStrain;
  }

  static std::vector<Strain> const & lowToHigh() {
    static const std::vector<Strain> lowToHighStrain = {
      Strain(0), Strain(3), Strain(2), Strain(1), Strain(0)
    };
    return lowToHighStrain;
  }
};


class Suit : public StrainT<false /* NoTrump illegal */> {
public:
  using StrainT::StrainT; // inherit all constructors (C++11)

public:
  static const int numCards = 13;

public:
  static std::vector<Suit> const & highToLow() {
    static const std::vector<Suit> highToLowSuit = {
      Suit(0), Suit(1), Suit(2), Suit(3)
    };
    return highToLowSuit;
  }

  static std::vector<Suit> const & lowToHigh() {
    static const std::vector<Suit> lowToHighSuit = {
      Suit(3), Suit(2), Suit(1), Suit(0)
    };
    return lowToHighSuit;
  }
};

typedef SpecializeOptional<Suit> OptionalSuit;

typedef SpecializeOptional<Strain> OptionalStrain;



//
// [Rank] and [AnyRank]
//
// The general idea in DDS is to represent the Rank of a card from 2 for Deuce
// up to 14 for Ace.  Though those are the only legal values for a literal
// card rank in a deck, the unused values can occur in some contexts.  (Most
// notably 0 has signaled "no card" in some cases.)  But meanings for 15 and 1
// may occur.
//
// The template has the flexibility to define special types for RankWithZero,
// RankWithOne, and RankWithFifteen (or any combination thereof).  For now
// only going with Rank and AnyRank.
//

template<
  bool zeroOk = true,
  bool oneOk = true,
  bool fifteenOk = true
>
class RankT {
protected:
  unsigned char value;

  template<class T> friend class SpecializeOptional;
  static const unsigned char optionalValue = 0xBD;
  void setOptional() {
     value = optionalValue;
  }
  bool testOptional() const {
     return value == optionalValue;
  }

  enum limitType {
    minValue = zeroOk ? 0 : (oneOk ? 1 : 2),
    maxValue = fifteenOk ? 15 : 14
  };

#if DDS_CHECKING_CARDDEFS
  void checkValid() const {
    assert(
      (value <= RankT::max())
      && ((value != 0) ? true : zeroOk)
      && ((value != 1) ? true : oneOk)
      && ((value != 15) ? true : fifteenOk)
    );
  }
#endif


/// RANK: LEGACY DDS 2.x INTEGER INTERFACE

public:
  static constexpr limitType min() { return minValue; }
  static constexpr limitType max() { return maxValue; }

  RankT (limitType limit) : value (limit) {}

  RankT & operator= (limitType limit) {
    value = limit;
    return *this;
  }

  RankT & operator++ (int) { // int means postfix
    // No validity check
    value++;
    return *this;
  }

  RankT operator-- (int) { // int means postfix
    // No validity check
    value--;
    return *this;
  }


/// RANK: MODERN C++11 INTERFACE

public:
  RankT ()
#if DDS_CHECKING_CARDDEFS
    : value (0xAE) // garbage value if checking, else don't pay to initialize
#endif
  {
  }

  explicit RankT (unsigned char value) :
    value (value)
  {
    DDS_CARDDEFS_CHECKVALID();
  }

public:
  bool operator<= (limitType limit) const {
    // No validity check
    return value <= limit;
  }

  bool operator>= (limitType limit) const {
    // No validity check
    return value >= limit;
  }

public:
  unsigned char toNumber() const {
    DDS_CARDDEFS_CHECKVALID();
    return value;
  }

public:
  char toChar() const {
    DDS_CARDDEFS_CHECKVALID();

    // 'z' => zero
    // 'o' => one
    // 'f' => fifteen
    static unsigned char const cardRank[16] = {
       'z', 'o', '2', '3', '4', '5', '6', '7',
       '8', '9', 'T', 'J', 'Q', 'K', 'A', 'f'
     };
     return cardRank[value];
  }
};


class Rank :
  public RankT<false /* no 0 */, false /* no 1 */, false /* no 15 */>
{
  using RankT::RankT; // inherit all constructors (C++11)

  // Rank is the only type that will implicitly coerce to an integer without
  // calling toNumber.  It strikes me as too dangerous for the weird AnyRank
  // values to be leaking 0s, 1s, and 15s when they are used to index into
  // arrays with max indices of 14 and so on.
public:
  operator unsigned char() const {
    DDS_CARDDEFS_CHECKVALID();
    return value;
  }

public: // Eventually private and only used by RankSet!
  friend class RankSet;
  unsigned short bitMask() const {
    DDS_CARDDEFS_CHECKVALID();

    static unsigned short const bitMaskRankData[16] = {
      0xAE00, // no RankSet mask for "fake" rank 0, garbage if unchecked
      0xAE01, // no RankSet mask for "fake" rank 1, garbage if unchecked
      0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020,
      0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0x0800, 0x1000,
      0xAE0F // no RankSet mask for "fake rank 15, garbage if unchecked
    };
    return bitMaskRankData[value];
  }

public:
  static std::vector<Rank> const & highToLow() {
    static const std::vector<Rank> highToLowRank = {
      Rank(14), Rank(13), Rank(12), Rank(11), Rank(10), Rank(9),
      Rank(8), Rank(7), Rank(6), Rank(5), Rank(4), Rank(3), Rank(2)
    };
    return highToLowRank;
  }

  static std::vector<Rank> const & lowTohigh() {
    static const std::vector<Rank> lowToHighRank = {
      Rank(2), Rank(3), Rank(4), Rank(5), Rank(6), Rank(7), Rank(8),
      Rank(9), Rank(10), Rank(11), Rank(12), Rank(13), Rank(14)
    };
    return lowToHighRank;
  }
};


class AnyRank :
  public RankT<true /* 0 ok */, false /* 1 ok */, false /* 15 ok */>
{
  using RankT::RankT; // inherit all constructors (C++11)

public:
  // No *implicit* coercion to integer given for this, you have to call the
  // toNumber().  But you can also add special functions here which might come
  // in handy.  This type of accessor is helpful...for instance if you are
  // using an AnyRank into an array whose max index is 13.
  unsigned char toLessThan14() const {
    assert(value < 14);
    return value;
  }
};


namespace std { namespace experimental {
  template<>
  class optional<Rank> : public ::SpecializeOptional<Rank> {
    using SpecializeOptional<Rank>::SpecializeOptional;
  };

  template<>
  class optional<AnyRank> : public ::SpecializeOptional<AnyRank> {
    using SpecializeOptional<AnyRank>::SpecializeOptional;
  };
} }



//
// [Priority]
//
// The Priority is the power of a card in a hand.  The highest card has
// priority 1, the lowest priority a card could have would be if you had
// all the cards in a suit and the 2 would be priority 14.  If you had
// only the king and the 7 then the king would be priority 1 and the 7
// would be priority 2
//
// The way the code is written at the moment, zero is not a valid
// priority and is unused.  For rationale on why lower numbers should
// mean higher "priority" in computer systems:
//
//    http://programmers.stackexchange.com/q/77365/944
//
// This concept used to be called "RelRank" for "Relative Rank" which
// was more confusing.  It and permitted 0 in some cases as a low value
// to mean things like "card isn't actually in hand".  With a separate
// OptionalPriority that won't be necessary...and perhaps priority values
// could be made zero-based for easier indexing in arrays?
//

class Priority {
private:
  unsigned char value;

  template<class T> friend class SpecializeOptional;
  static const unsigned char optionalValue = 0xBD;
  void setOptional() {
     value = optionalValue;
  }
  bool testOptional() const {
     return value == optionalValue;
  }

#if DDS_CHECKING_CARDDEFS
  void checkValid() const {
    assert((value != 0) && (value <= 14));
  }
#endif

public:
  Priority ()
#if DDS_CHECKING_CARDDEFS
    : value (0xAE) // garbage value if checking, else leave uninitialized
#endif
  {
  }

  explicit Priority (unsigned char value) :
    value (value)
  {
    DDS_CARDDEFS_CHECKVALID();
  }

  unsigned char toNumber() const {
    DDS_CARDDEFS_CHECKVALID();
    return value;
  }

  operator unsigned char() const {
    DDS_CARDDEFS_CHECKVALID();
    return value;
  }
};


namespace std { namespace experimental {
  template<>
  class optional<Priority> : public ::SpecializeOptional<Priority> {
    using SpecializeOptional<Priority>::SpecializeOptional;
  };
} }



//
// [Direction]
//
// Terminology-wise, both "Direction" and "Position" can be used to talk
// about "North, East, South, West".  Position is a more generic word,
// however, used in definitions like:
//
//   seat - Position relative to the dealer: for example, dealer's LHO
//          is said to be in second seat.
//
// "Hand" or "Player" might also be used.  But Direction is a very specific
// term that in the program context can only mean literally N, S, E, W.
// And Position is useful for talking about someone's position relative
// to more than just the dealer (e.g. "in the second position relative to
// the person leading the trick).
//
// Historically in DDS variables were named calling N, S, E, W the "hand".
// However that is more valuable as the name for an abstraction representing
// a state of cards held in a player's hand at a moment in time.
//
// As with Rank, there is a need for an optional variant of a Direction
// which uses no additional space.  A similar technique is used with fewer
// variations, and a magic number 0xBD to signal optionality.
//

class Position; // Forward definition

class Direction {
private:
  unsigned char value;

  template<class T> friend class SpecializeOptional;
  static const unsigned char optionalValue = 0xBD;
  void setOptional() {
     value = optionalValue;
  }
  bool testOptional() const {
     return value == optionalValue;
  }

  enum limitType {
    minValue = 0,
    maxValue = 3
  };

public:
  static constexpr limitType min() { return minValue; }
  static constexpr limitType max() { return maxValue; }

  Direction (limitType limit) : value (limit) {}

  bool operator<= (limitType limit) const {
    // No validity check
    return value <= limit;
  }

private:
#if DDS_CHECKING_CARDDEFS
  void checkValid() const {
    assert(value <= Direction::max());
  }
#endif

public:
  Direction ()
#if DDS_CHECKING_CARDDEFS
    : value (0xAE) // garbage value if checking, else leave uninitialized
#endif
  {
  }

  explicit Direction (unsigned char value) :
    value (value)
  {
    DDS_CARDDEFS_CHECKVALID();
  }

  // REVIEW: How to name the enumeration orders?  I personally like a
  // more precise name to just saying "all directions".  This name is
  // just an idea...there are of course lots of ways to enumerate (including
  // the legacy way with for loops and ++)
  static std::vector<Direction> const & NESW() {
    static const std::vector<Direction> neswDirection = {
      Direction(0), Direction(1), Direction(2), Direction(3)
    };
    return neswDirection;
  }

  // "Left hand opponent"
  Direction lho() const {
    DDS_CARDDEFS_CHECKVALID();
    static Direction const lhoData[4] = {
      Direction(1), Direction(2), Direction(3), Direction(0)
    };
    return lhoData[value];
  }

  // "Right hand opponent"
  Direction rho() const {
    DDS_CARDDEFS_CHECKVALID();
    static Direction const rhoData[4] = {
      Direction(3), Direction(0), Direction(1), Direction(2)
    };
    return rhoData[value];
  }

  Direction partner() const {
    DDS_CARDDEFS_CHECKVALID();
    static Direction const partnerData[4] = {
      Direction(2), Direction(3), Direction(0), Direction(1)
    };
    return partnerData[value];
  }

  char toChar() const {
    DDS_CARDDEFS_CHECKVALID();
    static char const directionChars[4] = {
      'N', 'E', 'S', 'W'
    };
    return directionChars[value];
  }

  std::string toString() const {
    DDS_CARDDEFS_CHECKVALID();
    static std::string const directionStrings[4] = {
      "North", "East", "South", "West"
    };
    return directionStrings[value];
  }

  unsigned char toNumber() const {
    DDS_CARDDEFS_CHECKVALID();
    return value;
  }

  operator unsigned char() const {
    return toNumber();
  }

  Direction & operator++ (int) { // int means postfix
    value++;
    return *this;
  }

  Direction operator-- (int) { // int means postfix
    value--;
    return *this;
  }

  Direction operator+ (Position const & relative);
};


namespace std { namespace experimental {
  template<>
  class optional<Direction> : public ::SpecializeOptional<Direction> {
    using SpecializeOptional<Direction>::SpecializeOptional;
  };
} }



//
// [Position]
//
// A Position is first, second, third or fourth.  It is relative to another
// "Direction"...which can be for any purpose.  If it is relative to the
// dealer then the term would be a "Seat".  But it can also be relative to
// the declarer or any Direction at the table for any reason.
//
// Technically, positions could be processed by cycling around the table
// an arbitrary number of times.  So you could ++ a position at 3 and get 0,
// and then keep doing this.  The value of that in the program is quesitonable
// so it is probably best to say that once a position has been established
// it doesn't roll over...unless an express need for that is established.
//
// The most useful application of a position is to be able to apply it to
// a Direction, so you can say "Direction(South) + Position(2)" is
// Direction(North).  At the moment, this is accomplished with an actual
// addition operator.  There is a constructor for a position which takes
// two hands which is the preferred method for generating them.
//

class Position {
private:
  unsigned char value;

  template<class T> friend class SpecializeOptional;
  static const unsigned char optionalValue = 0xBD;
  void setOptional() {
     value = optionalValue;
  }
  bool testOptional() const {
     return value == optionalValue;
  }

#if DDS_CHECKING_CARDDEFS
  void checkValid() const {
    assert(value <= 3);
  }
#endif

public:
  Position ()
#if DDS_CHECKING_CARDDEFS
    : value (0xAE) // garbage value if checking, else leave uninitialized
#endif
  {
  }

  Position (unsigned char value)
    : value (value)
  {
     DDS_CARDDEFS_CHECKVALID();
  }

  // presume this leading hand, "relative" is hand relative leading hand.
  Position (Direction const & leading, Direction const & relative) :
    value ((leading + relative) & 3)
  {
    DDS_CARDDEFS_CHECKVALID();
  }

  Position & operator++ (int) { // int means postfix
    value++;
    return *this;
  }

  Position operator-- (int) { // int means postfix
    value--;
    return *this;
  }

  // REVIEW: This is used by ABSearch's Undo, should it be?  I verified it
  // actually does want to roll over from 0 back to 3  :-/
  Position oneBackwards() const {
    DDS_CARDDEFS_CHECKVALID();
    return Position((value + 3) & 0x3);
  }

  unsigned char toNumber() const {
    DDS_CARDDEFS_CHECKVALID();
    return value;
  }

  // REVIEW: Is implicit casting to an integer necessary for this class?
  operator unsigned char() const {
    return toNumber();
  }

  char toChar() const {
    DDS_CARDDEFS_CHECKVALID();
    static char const positionChars[4] = {
      '1', '2', '3', '4'
    };
    return positionChars[value];
  }

  std::string toString() const {
    DDS_CARDDEFS_CHECKVALID();
    static std::string const positionStrings[4] = {
      "First", "Second", "Third", "Fourth"
    };
    return positionStrings[value];
  }
};

inline Direction Direction::operator+ (
  Position const & relative
) {
  DDS_CARDDEFS_CHECKVALID();
  return Direction((value + relative.toNumber()) & 3);
}


namespace std { namespace experimental {
  template<>
  class optional<Position> : public ::SpecializeOptional<Position> {
    using SpecializeOptional<Position>::SpecializeOptional;
  };
} }



//
// [RankSet]
//
// A class for representing a set of ranks; possibly all of them and
// possibly also empty.  It is designed specifically for the case of storing
// and processing Deuce through Ace, so it cannot be used with an AnyRank
// or other Rank variant.
//
// It uses four static tables for speedup that are initialized once.
//
// The API is modeled after after the std::bitset, whose terminology is
// a bit different than the other collections.  It has `none()` instead
// of `empty()` if there are no members, and `any()` as a special check
// equivalent to `!none()`:
//
//    http://www.cplusplus.com/reference/bitset/bitset/
//
// Instead of bit indexes, the RankSet is indexed by a Rank.  Operations
// for "setting", "resetting", and "flipping" the status of a Rank are
// modifying (as in bitset) while returning the modified bitset.  Bit
// masking operations operate in terms of other RankSets...not a single
// Rank.
//
// BUT the idea of + and - are not generally sensible operations on bitsets.
// So when looking at the code I see some bit flipping and OR'ing where
// it would seem that it might help if there were a debug version that
// checked to make sure that you were actually removing a rank from a set
// or adding it.  So borrowing + and - for these is a concept I am
// throwing out there.  If you want to add a Rank to a RankSet you can
// just write:
//
//    rankset += rank;
//
// Or more generally:
//
//    rankset = rankset + rank;
//
// Note however that will be asserting that the rank is not already in the
// RankSet.  The reverse is true for subtraction.  If you *actually* want to
// do ands and ors and nots and XORs you will have to do explicit construction
// of a RankSet from a Rank and use that.
//

class RankSet {
private:
  unsigned short value;

  template<class T> friend class SpecializeOptional;
  static const unsigned short optionalValue = 0xBFD3;
  void setOptional() {
     value = optionalValue;
  }
  bool testOptional() const {
     return value == optionalValue;
  }

  enum limitType {
    minValue = 0,
    maxValue = ((1 << Suit::numCards) - 1) // 8191
  };

#if DDS_CHECKING_CARDDEFS

  void checkValid() const {
    assert(value <= RankSet::max());
  }
#endif


/// RANKSET: LEGACY DDS 2.x INTEGER INTERFACE

public:
  static constexpr limitType min() { return minValue; }
  static constexpr limitType max() { return maxValue; }

  explicit RankSet (unsigned short value) :
    value (value)
  {
    DDS_CARDDEFS_CHECKVALID();
  }

  RankSet (limitType limit) : value (limit) {}

  bool operator<= (limitType limit) const {
    return value <= limit;
  }

#ifdef CRAZY_ROLLOVER_IDEA
  bool operator>= (limitType limit) const {
    // Idle thought to allow signed rollover; so you can start from max()
    // and decrement down to the limit of min() and subtract past it.
    // Probably a bad idea, but lets you enumerate from high to low with
    // integer legacy.  Produces optional values on the boundary; could
    // be mitigated with a different choice for optional.  Just an idea.
    if (limit == min()) {
      return !(value > max());
    } else {
      return value >= limit;
    }
  }
#endif

  RankSet & operator++ (int) { // http://stackoverflow.com/q/3574831
    value++; return *this;
  }

  RankSet & operator-- (int) { // http://stackoverflow.com/q/3574831
    value--; return *this;
  }

  // Used internally, but shouldn't be exported in the long term!
  operator unsigned short() const {
    DDS_CARDDEFS_CHECKVALID();
    return value;
  }


/// RANKSET: MODERN C++11 INTERFACE

public:
  RankSet ()
    : value (0)
  {
  }

  explicit RankSet (Rank const & rank) :
    value (rank.bitMask())
  {
    DDS_CARDDEFS_CHECKVALID();
  }

  bool test(Rank const & rank) const {
     DDS_CARDDEFS_CHECKVALID();
     return value & rank.bitMask();
  }

  std::experimental::optional<Rank> toSingleRank() const {
      DDS_CARDDEFS_CHECKVALID();
      switch (value) {
        case 0x1000: return Rank(14);
        case 0x0800: return Rank(13);
        case 0x0400: return Rank(12);
        case 0x0200: return Rank(11);
        case 0x0100: return Rank(10);
        case 0x0080: return Rank(9);
        case 0x0040: return Rank(8);
        case 0x0020: return Rank(7);
        case 0x0010: return Rank(6);
        case 0x0008: return Rank(5);
        case 0x0004: return Rank(4);
        case 0x0002: return Rank(3);
        case 0x0001: return Rank(2);
        case 0x0000: return std::experimental::nullopt;
        // if more than one rank, we could throw an error, but for the moment
        // it is the same answer as if a RankSet is empty.
        default: return std::experimental::nullopt;
      }
  }

  std::experimental::optional<Rank> highestRank() const {
    DDS_CARDDEFS_CHECKVALID();

    static std::experimental::optional<Rank> data[RankSet::max() + 1];
    static bool initialized = false;
    if (!initialized) {
      for (RankSet rs = RankSet::min(); rs <= RankSet::max(); rs++)
      {
        for (Rank r = Rank::max(); r >= Rank::min(); r--)
        {
          if (rs.test(r))
          {
            data[rs] = r;
            break;
          }
        }
      }
      initialized = true;
    }
    return data[value];
  }

  unsigned char count() const {
    DDS_CARDDEFS_CHECKVALID();

    static unsigned char data[RankSet::max() + 1];
    static bool initialized = false;
    if (!initialized) {
      for (RankSet rs = RankSet::min(); rs <= RankSet::max(); rs++)
      {
        unsigned char result = 0;
        for (Rank r = Rank::max(); r >= Rank::min(); r--)
        {
          if (rs.test(r))
          {
            result++;
          }
        }
        data[rs] = result;
      }
      initialized = true;
    }
    return data[value];
  }

  // Same as checking .count()==0, but slightly faster
  bool none() const {
    return value == 0;
  }

  // Same as checking .count()>0, but slightly faster
  bool any() const {
    return value != 0;
  }

  std::experimental::optional<Priority> priorityIfInHand(
    Rank const & rank
  ) const {
    DDS_CARDDEFS_CHECKVALID();

    int rankValue = rank.toNumber();
    if (rankValue < 2) // toNumber() should have guarded against 1 in Debug
      return std::experimental::nullopt;

    static unsigned char data[RankSet::max()+1][Rank::max()-Rank::min()+1];
    static bool initialized = false;
    if (!initialized) {
      for (RankSet rs = RankSet::min(); rs <= RankSet::max(); rs++)
      {
        unsigned char ord = 0;
        for (Rank r = Rank::max(); r >= Rank::min(); r--)
        {
          if (rs.test(r))
          {
            ord++;
            data[rs][r - Rank::min()] = ord;
          }
          else
          {
            data[rs][r - Rank::min()] = 0;
          }
        }
      }

      initialized = true;
    }
    unsigned char const & temp = data[value][rankValue - Rank::min()];
    if (temp == 0)
       return std::experimental::nullopt;
    return Priority(temp);
  }

  // Use this only if you know the rank is in the RankSet
  Priority priority(Rank const & rank) const {
     return *priorityIfInHand(rank);
  }

  unsigned short toNumber() const {
    DDS_CARDDEFS_CHECKVALID();
    return value;
  }

  // winRanks is the absolute suit represented by the RankSet, limited to its
  // top "leastWin" bits.  Valid values for leastWin are 0-13, hence this
  // operation is apparently speaking
  RankSet const & winRanks(unsigned char leastWin) const {
    DDS_CARDDEFS_CHECKVALID();

    static RankSet const emptyRankSet (0);
    if (leastWin == 0)
       return emptyRankSet;

    assert(leastWin < 14);

    static bool initialized = false;
    static RankSet winRanksData[RankSet::max() + 1][Suit::numCards];
    if (!initialized) {
      for (RankSet rs = RankSet::min(); rs <= RankSet::max(); rs++)
      {
        for (int lw = 1; lw < 14; lw++)
        {
          unsigned short nextBitNo = 1;
          RankSet result;
          for (Rank r = Rank::max(); r >= Rank::min(); r--)
          {
            if (rs.test(r))
            {
              if (nextBitNo <= lw)
              {
                result += r;
                nextBitNo++;
              }
              else
                break;
            }
          }
          winRanksData[rs][lw - 1] = result;
        }
      }
      initialized = true;
    }
    return winRanksData[value][leastWin - 1];
  }

public:
  bool operator== (RankSet const & other) const {
    DDS_CARDDEFS_CHECKVALID();
    return value == other.toNumber();
  }

  bool operator!= (RankSet const & other) const {
    DDS_CARDDEFS_CHECKVALID();
    return value != other.toNumber();
  }

  // Produce an RankSet that is the least significant bit set for comparison
  RankSet toLowest() {
    DDS_CARDDEFS_CHECKVALID();

    // LSB
    return RankSet(value & (-value));
  }

  RankSet & operator|= (RankSet const & other) {
    DDS_CARDDEFS_CHECKVALID();
    value |= other.toNumber();
    return *this;
  }

  RankSet & operator&= (RankSet const & other) {
    DDS_CARDDEFS_CHECKVALID();
    value &= other.toNumber();
    return *this;
  }

  RankSet & operator^= (RankSet const & other) {
    DDS_CARDDEFS_CHECKVALID();
    value ^= other.toNumber();
    return *this;
  }

  RankSet & reset(Rank const & rank) {
     DDS_CARDDEFS_CHECKVALID();
     value &= ~rank.bitMask();
     return *this;
  }

  RankSet & reset() {
     DDS_CARDDEFS_CHECKVALID();
     value = 0;
     return *this;
  }

  RankSet & set(Rank const & rank) {
    DDS_CARDDEFS_CHECKVALID();
    value |= rank.bitMask();
    return *this;
  }

  RankSet & set() {
    DDS_CARDDEFS_CHECKVALID();
    value = RankSet::max();
    return *this;
  }

  RankSet & flip(Rank const & rank) {
    DDS_CARDDEFS_CHECKVALID();
    value ^= rank.bitMask();
    return *this;
  }

  RankSet & flip() {
    DDS_CARDDEFS_CHECKVALID();
    value = ~value & RankSet::max();
    return *this;
  }

  RankSet operator~() const {
     DDS_CARDDEFS_CHECKVALID();
     return RankSet(~value & RankSet::max());
  }

  // DDS algorithms check to see if one RankSet is greater than another
  // in a value way...e.g. if there is an available card which can beat
  // the other RankSet.  Because it's cheaper to turn a Rank into a RankSet
  // than vice versa, what is intended as a "which Rank is higher" operation
  // often involves switching a Rank into the RankSet domain...and the
  // operation of cloning only the least significant bit is used to do
  // some quick operations.  It is probably possible to come up with a
  // more legible way of capturing the operation that's actually being done
  // if the cases are studied a little.

  RankSet cloneOnlyLSB() {
    DDS_CARDDEFS_CHECKVALID();
    return RankSet(value & (-value));
  }

  bool operator< (RankSet const & other) {
    DDS_CARDDEFS_CHECKVALID();
    return value < other.toNumber();
  }

  bool operator> (RankSet const & other) {
    DDS_CARDDEFS_CHECKVALID();
    return value > other.toNumber();
  }


/// SPECIAL "ASSERTING" BIT SETTING OPERATIONS

  RankSet & operator-= (RankSet const & other) {
    DDS_CARDDEFS_CHECKVALID();
    assert((other.value & value) == other.value);
    value &= ~other.toNumber();
    return *this;
  }

  RankSet & operator+= (RankSet const & other) {
    DDS_CARDDEFS_CHECKVALID();
    assert(!(other.value & value));
    value |= other.toNumber();
    return *this;
  }

  RankSet & operator-= (Rank const & rank) {
    DDS_CARDDEFS_CHECKVALID();
    assert(rank.bitMask() & value);
    value &= ~rank.bitMask();
    return *this;
  }

  RankSet & operator+= (Rank const & rank) {
    DDS_CARDDEFS_CHECKVALID();
    assert(!(rank.bitMask() & value));
    value |= rank.bitMask();
    return *this;
  }


/// GLOBAL SCOPE BINARY BITSET OPERATORS

// Here's why it's good to be implementing + in terms of += (etc.) like this
// http://stackoverflow.com/a/5046346/211160

  friend RankSet operator+ (RankSet lhs, RankSet const & rhs) {
    return lhs += rhs;
  }

  friend RankSet operator- (RankSet lhs, RankSet const & rhs) {
    return lhs -= rhs;
  }

  friend RankSet operator| (RankSet lhs, RankSet const & rhs) {
    return lhs |= rhs;
  }

  friend RankSet operator^ (RankSet lhs, RankSet const & rhs) {
    return lhs ^= rhs;
  }

  friend RankSet operator& (RankSet lhs, RankSet const & rhs) {
    return lhs &= rhs;
  }
};


namespace std { namespace experimental {
  template<>
  class optional<RankSet> : public ::SpecializeOptional<RankSet> {
    using SpecializeOptional<RankSet>::SpecializeOptional;
  };
} }



//
// [Card]
//
// A class for representing a card with a rank and a suit, members public
// and initialization required.  Nothing uses it at the moment but it
// might come in handy.
//
class Card {
public:
  Rank rank;
  Suit suit;

public:
  Card (Rank const & rank, Suit const & suit) :
    rank (rank),
    suit (suit)
  {
  }
};



//
// [Hand]
//
// A hand is an abstraction representing a set of cards held in a hand at a
// point in time.  It is the belief that this definition would be useful as
// a class that led me to take "Direction" for N,S,E,W instead of calling
// that "Hand".
//
class Hand {
protected:
  std::array<RankSet, Suit::max() + 1> ranksForSuit;

public:
  // Make constructors for PBN etc?
};


#endif
