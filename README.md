# cara
Cara is a compiler my language "Tara".
The language is primarily an experiment.

The aim is a rust-level (if not lower) language, that incorporates an even richer type system a la Haskell.
The idea was sparked by not being able to write:
```rs
struct Bar<C> {
    baz: usize,
    branch_a: Box<Bar<C>>,
    branch_b: C<Foo>
}
```
That is, rust lacking support for true higher kinded types.

# docs
Currently the little documentation that does exist is scattered all over in my personal files, cloud apps, devices, etc...
