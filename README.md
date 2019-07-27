# curl-wrapper - A decent C++ curl wrapper

## Usage
- Add `curl-wrapper.h` and `curl-wrapper.cpp` to your project.
- Call `Curl::Initialize` to initialize curl-wrapper and curl. (It can safely be called multiple time)
- When it's time to shutdown `Curl::Shutdown`. (It must be call as many time as `Curl::Initialize`)
- Don't hesitate to modify, especially use more optimized data structure if you have.

## Motivation

Although, I don't really care that people use terrible libraries, I was
recently highly affected by the existence of few of those libraries.
More specifically, [curlpp](https://github.com/jpbarrette/curlpp). Indeed,
even though curl is clearly object oriented, well documented, stable, portable
and trivial to use, the C++ community often feel the needs to make wrappers to do
`obj.func(args)` instead of `func(obj, args)`. I wouldn't be honest if I didn't
mention that one problem with curl is the lack of type-safety when setting an
option with [curl_easy_setopt](https://curl.haxx.se/libcurl/c/curl_easy_setopt.html).
This wasn't done without reason, it allow to be binary compatible between
version. That said, if one want to make a wrapper of curl (I personally see
very little advantage), this person have to put a lot of care into bringing
advantages without bringing too much drawbacks if any. In my opinion, curlpp
miserably fail to do so and I will illustrate why I think so.

## Problems with curlpp

1. The first problem is intrinsic with any library, especially in C or C++.
Adding new code introduces new bugs, more complexity, more indirection, more
compile time, relying on more people(s), etc.

2. The second problem is headers pull-in. For instance, `CurlEasy.hpp`
pulls in:

```
curl.h x 6

<cassert>
<functional>
<iostream>
<list> x 3
<map>
<memory> x 2
<string> x 5
<stdexcept> x 2

CurlHandle.hpp x 2
Exception.hpp x 3
Form.hpp
Option.hpp
OptionBase.hpp x 2
OptionContainer.hpp
OptionContainerType.hpp x 2
OptionList.hpp
OptionSetter.hpp
SList.hpp
Types.hpp x 3

clone_ptr.hpp
```

This is not necessary a problem, but a trade of where compile time,
usability and complexity are compared. In this particular case, it's
not necessary to have such complex dependency as demonstrated with
my wrapper. Interestingly enough, although I think the namespace in C++
are broken by design, I agree with the advantages provided by them. In
the case of curlpp, those advantages are all lost as soon as you include
`curl.h`.

3. The third problem is the bad usage of dynamic allocations. Indeed,
in language such as C#, Java, etc. it would be counter productive to
think too much about the memory allocation, because the lack of control.
It's a choice that is made when choosing such a language. In a wrapper
of a perfectly good library, it's unacceptable to add this complexity
for no reason.

I will start with an example that can be discarded, because it's not
exactly from the library. This example come from an example, but I
think it's still relevant. The examples are what people base their
code on when they use the library. Moreover, it may show or explain
some decisions made in the library.
```cpp
request.setOpt(new Verbose(true));
request.setOpt(new ReadStream(&myStream));
request.setOpt(new InfileSize(size));
request.setOpt(new Upload(true));
request.setOpt(new HttpHeader(headers));
request.setOpt(new Url(url));
```

Why would you need to do a dynamic allocation for a bool or a int ?
Bonus: Memory is leaked.

The second example which is from the library is when you create an easy
handle. To create an handle with curl you need to call
[curl_easy_init](https://curl.haxx.se/libcurl/c/curl_easy_init.html).
This forces a dynamic allocation, but it has the advantages of having
a stable ABI, it's easy to pass around and there is much less bugs
possible, because curl has a say before the memory is freed. Because of
that, a wrapper could boils down to having this pointer in. Instead,
`curlpp::Easy` constructor call `new internal::CurlHandle()` which then
call `curl_easy_init`.

The last problem that I will mention is when one set an option. Ignoring
the mess that it is to go through the code bases, because I will assume
it's my lack of knowledge, setting any option will create a temporary
`Option` object that will have a pointer to a `OptionContainer` which
is created with:
```cpp
// 'typename Option<OptionType>::ParamType' is a reference here
template<typename OptionType> void
Option<OptionType>::setValue(typename Option<OptionType>::ParamType value)
{
    if (mContainer == NULL)
        mContainer = new internal::OptionContainer<OptionType>(value);
    else
        mContainer->setValue(value);
}
```

But it's not over here:
```cpp
template<class OptionType>
OptionContainer<OptionType>::OptionContainer(
    typename OptionContainer<OptionType>::ParamType value)
: mValue(value)
{
}
```

So the line `request.setOpt(new Url(url));` was:
* 1 dynamic allocation + string copy for `new Url(url)`
* 1 dynamic allocation to create the `OptionContainer`
* 1 dynamic allocation + string copy to put the string in `OptionContainer`.
* A potential additional cost, because `std::map` is used to store the options.

Moreover, in `curl_easy_setopt` documentation we have the following note:
> Strings passed to libcurl as 'char *' arguments, are copied by the library;

So why in the hell does he even bother. It could boils down to calling
`curl_easy_setopt` with a bit of type-safety and no extra allocations/copies!

4. This is similar to 3, the difference is that one could argue that it's
a cost that is worth paying for the advantages that it brings. Some options
such as the urls are strings. `curl_easy_setopt` expect a `const char *`,
but curlpp forces you to use `std::string`. One could argue against null-terminated
string, but in this case you either pay nothing (you already had a `std::string`)
or you pay (you have to construct a std::string).

5. One of the *annoyance* in curl is you can only set one option at the time.
A typical example of this are the function and there user data.
```c
curl_easy_setopt(handle, CURLOPT_WRITEDATA, ctx);
curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, handle_write_data);
```
Why not providing a function such as:
```cpp
bool SetWriteCallback(Callback callback, void *param = nullptr);
```

Or an other example is [CURLOPT_READFUNCTION](https://curl.haxx.se/libcurl/c/CURLOPT_READFUNCTION.html)
for data upload. Curl does that, because it's the only way to stay general
and cover every cases. But, often one want to upload a small buffer that is
in memory. That's the kind of thing I value in a wrapper.
e.g:
```cpp
bool SetUploadBuffer(const void *buffer, size_t size);
```

Those are ideas and may or may not be wanted in curlpp, but the naive way
that was chosen only increase the friction by adding new types and a lot
of dynamic allocations (as we saw earlier) for no gains whatsoever. Well,
a bit of type-safety that could have been done by generating automatically
a bunch of setters in a header from `enum CURLoption` that specify the
options type.
e.g:
```c
typedef enum {
  /* This is the FILE * or void * the regular output should be written to. */
  CINIT(WRITEDATA, OBJECTPOINT, 1),

  /* The full URL to get/put */
  CINIT(URL, STRINGPOINT, 2),

  /* Port number to connect to, if other than default. */
  CINIT(PORT, LONG, 3),

  /* Name of proxy to use. */
  CINIT(PROXY, STRINGPOINT, 4),

  // ...
} CURLoption;
```

6. This problem is not necessary a problem, because it boils down to
the argument of exception vs no exception. Curlpp use exceptions which
can be a problem if you want a code base without exception.

7. Why is `setOpt` virtual ?

There is few other problems that could be pointed out, but I think I made
my case. This library is a Computer Science homework and that's not a problem
until it start being used in professional project. For instance,
[fffaraz/awesome-cpp](https://github.com/fffaraz/awesome-cpp)
offer a list of so-called:
> A curated list of awesome C++ (or C) frameworks, libraries, resources,
and shiny things. Inspired by awesome-... stuff.

The problem here, is that he recommends curlpp, so how can I trust any of the
other libraries that he recommends? This also means that I can't trust the
judgment of any of the person in the ~20k stars without further informations.

## Solution
In an effort to avoid dealing with curlpp in the future, I will provide a
wrapper that should give you a better starting point. It's a simple two
files (.h, .cpp), that you should easily be able to modify. Not all options
are covered, but they are trivial to add. That said, think twice to avoid
adding them naively.
