# Five Words ()

The awesome response to the [Stand-up Maths](https://www.youtube.com/watch?v=c33AZBnRHks) generated many solutions.

The best one unti I'm writing this code is from [miniBill](https://github.com/miniBill/parkerrust) which was writen in Rust.

I'm a Rust newbie, but I know some of C++ and I took a couple of hours to "translate" the code from Rust to C++.

## What I'm using?

Pure C++ and boost::iostream to memory mapped file to be portable.

## Achievement

The same performance on my machine, using the same techiniques... It's very nice to see that Rust is as fast as C++ (your write less in Rust thow, but I find it more dificult to read... for now at least).

## TODO

Se what can be done to improve performance using:

1. Allocators
2. std::bitset
3. Better containers
4. Change the aproach for C++20 Ranges (it's almost incredible but some algorthms get almos 20% of improvement in C++20 Ranges style - don't ask me why)
5. ???