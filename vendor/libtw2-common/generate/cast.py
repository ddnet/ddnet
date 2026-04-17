from .types import TYPES, formatt, printt

prologue = """\
use std::convert::TryInto;
use std::fmt;

trait TypeName {
    fn type_name() -> &'static str;
}

#[cold]
#[inline(never)]
fn overflow<T: fmt::Display, U: TypeName>(original: T) -> ! {
    panic!("Overflow casting {} to `{}`", original, U::type_name());
}

#[inline]
fn unwrap_overflow<T: fmt::Display, U: TypeName>(original: T, val: Option<U>) -> U {
    match val {
        Some(v) => v,
        None => overflow::<T, U>(original),
    }
}\
"""

def print_prologue():
    print(prologue)

def print_cast_trait(types):
    print("pub trait Cast {")
    for t in types:
        printt("    fn {type}(self) -> {type} where Self: Sized + {trait};", t)
    for t in types:
        printt("    fn try_{type}(self) -> Option<{type}> where Self: Sized + {ntrait};", t)
    for t in types:
        printt("    fn assert_{type}(self) -> {type} where Self: Sized + {ntrait};", t)
    print("}")

def print_pos_traits(types):
    for t in types:
        printt("pub trait {trait} {{ }}", t)

def print_neg_traits(types):
    for t in types:
        printt("pub trait {ntrait} {{ }}", t)

def print_traits(types):
    print_cast_trait(types)
    print()
    print_pos_traits(types)
    print()
    print_neg_traits(types)

def minimum_bits(type):
    size = type[1:]
    if size == "size":
        return 32
    return int(size)

def maximum_bits(type):
    size = type[1:]
    if size == "size":
        return 64
    return int(size)

def can_always_represent(type1, type2):
    """
    Can `type1` always represent `type2`?
    """
    if type1 == type2:
        return True
    signedness1, size1 = type1[0], type1[1:]
    signedness2, size2 = type2[0], type2[1:]
    if signedness1 == signedness2:
        return minimum_bits(type1) >= maximum_bits(type2)
    elif signedness1 == "u" and signedness2 == "i":
        return False
    elif signedness1 == "i" and signedness2 == "u":
        return minimum_bits(type1) > maximum_bits(type2)
    raise RuntimeError("unreachable")

def print_pn_trait_impls_type(type, types):
    for t in types:
        if can_always_represent(type, t):
            print("impl {} for {} {{ }}".format(formatt("{trait}", type), formatt("{type}", t)))
        else:
            print("impl {} for {} {{ }}".format(formatt("{ntrait}", type), formatt("{type}", t)))

def print_pn_trait_impls(types):
    first = True
    for t in types:
        if not first:
            print()
        else:
            first = False
        print_pn_trait_impls_type(t, types)

def print_cast_impl(type, types):
    printt("impl Cast for {type} {{", type)
    for t in types:
        if can_always_represent(t, type):
            printt("    #[inline] fn {type}(self) -> {type} {{ self.try_into().ok().unwrap() }}", t)
        else:
            printt("    #[inline] fn {type}(self) -> {type} {{ unreachable!() }}", t)
    for t in types:
        if not can_always_represent(t, type):
            printt("    #[inline] fn try_{type}(self) -> Option<{type}> {{ self.try_into().ok() }}", t)
        else:
            printt("    #[inline] fn try_{type}(self) -> Option<{type}> {{ unreachable!() }}", t)
    for t in types:
        if not can_always_represent(t, type):
            printt("    #[inline] fn assert_{type}(self) -> {type} {{ unwrap_overflow(self, self.try_into().ok()) }}", t)
        else:
            printt("    #[inline] fn assert_{type}(self) -> {type} {{ unreachable!() }}", t)
    print("}")

def print_cast_impls(types):
    first = True
    for t in types:
        if not first:
            print()
        else:
            first = False
        print_cast_impl(t, types)

def print_type_name_impls(types):
    for t in types:
        printt("impl TypeName for {type} {{ fn type_name() -> &'static str {{ \"{type}\" }} }}", t)

def print_trait_impls(types):
    print_cast_impls(types)
    print()
    print_pn_trait_impls(types)
    print()
    print_type_name_impls(types)

def main():
    types = TYPES
    print_prologue()
    print()
    print_traits(types)
    print()
    print_trait_impls(types)

if __name__ == '__main__':
    main()
