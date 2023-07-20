gmi100
======

Gemini CLI protocol client written in 100 lines of ANSI C.

![demo.gif](demo.gif)

Other similar Gemini client projects written in few lines of code
successfully shows how simple Gemini protocol is.  This code is far
from straight forward.  But I had a different goal in mind.

I tried to pack as much as possible in 100 lines of ANSI C.  Initially
I struggled to fit simple TLS connection in such small space but
eventually I ended up with CLI client capable of efficient navigation
between capsules of Gemini space 🚀


Build, run and usage
--------------------

Run `build` script or use any C compiler and link with [OpenSSL][0].

	$ ./build               # Compile on Linux
	$ ./gmi100              # Run with default "less -XI" pager
	$ ./gmi100 more         # Run using "more" pager
	$ ./gmi100 cat          # Run using "cat" as pager

	gmi100> gemini.circumlunar.space

In `gmi100>` prompt you can take few actions:

1. Type Gemini URL to visit specific site.
2. Type a number of link on current capsule, for example: `12`.
3. Type `q` to quit.
4. Type `r` to refresh current capsule.
5. Type `u` to go "up" in URL directory path.
6. Type `b` to go back in browsing history.
7. Type `c` to print current capsule URI.
8. Type `?` to search, geminispace.info/search is used by default.
9. Type shell command prefixed with `!` to run it on current capsule.

Each time you navigate to `text` document the pager program will be
run with that file.  By default `less -XI` is used but you can provide
any other in first program argument.  If your pager is interactive
like less the you have to exit from that pager in order to go back to
gmi100 prompt and navigate to other capsule.

When non `text` file is visited, like an image or music then nothing
will be displayed but temporary file will be created.  Then you can
use any shell command to do something with it.  For example you can
visit capsule with video and open it with `mpv`:

	gmi100> gemini://tilde.team/~konomo/noocat.webm
	gmi100> !mpv

Or similar example with image and music.  For example you can use
`xdg-open` or `open` command to open file with default program for
given MIME type.

	gmi100> gemini://158.nu/images/full/158/2022-03-13-0013_v1.jpg
	gmi100> !xdg-open
	
You also can use any program on reqular text capsules.  For example
you decided that your defauly pager is `cat` but for some capsules you
want to use `less`.  Or you want to edit given page in text editor.
In summary, you can open currently loaded capsule as file in any
program as long as you don't navigate to other URI.

	gmi100> gemini.circumlunar.space
	gmi100> !less
	gmi100> !emacs
	gmi100> !firefox
	gmi100> !xdg-open


How browsing history works
--------------------------

Browsing history in gmi100 works differently than regular "stack" way
that is commonly used in browsers and other regular modern software.
It is inspired by how Emacs handles undo history.  That means with the
single "back" button you can go back and forward in browsing history.
Also with that you will never loose any page you visited from history
file and I was able to write this implementation in only few lines.

After you run the program it will open or create history .gmi100 file.
Then every page you visits that is not a redirection to other page and
doesn't ask you for input will be appended at the end of history file.
File is never cleaned up by program itself to make history persistent
between sessions but that means cleaning up browsing history is your
responsibility.  But this also gives you an control over history file
content.  You can for example append some links that you want to visit
in next session to have easier access to them just by running program
and pressing "b" which will navigate to last link from history file.

During browsing session typing "b" in program prompt for the first
time will result in navigation to last link in history file.  Then if
you type "b" again it will open second to last link from history.  But
it will also append that link at the end.  You can input "b" multiple
times and it will always go back by one link in history and append it
at then end of history file at the same time.  Only if you decide to
navigate to other page by typing URL or choosing link number you will
break that cycle.  Then history "pointer" will go back to the very
bottom of the history file.  Example:

	gmi100 session      pos  .gmi100 history file content
	==================  ===  ===============================
	
	gmi100>                  <EMPTY HISTORY FILE>
	
	gmi100> tilde.pink  >>>  tilde.pink
	
	gmi100> 2                tilde.pink
	                    >>>  tilde.pink/documentation.gmi
	
	gmi100> 2                tilde.pink
	                         tilde.pink/documentation.gmi
	                    >>>  tilde.pink/docs/gemini.gmi
	
	gmi100> b                tilde.pink
	                    >>>  tilde.pink/documentation.gmi
	                         tilde.pink/docs/gemini.gmi
	                         tilde.pink/documentation.gmi
	
	gmi100> b           >>>  tilde.pink
	                         tilde.pink/documentation.gmi
	                         tilde.pink/docs/gemini.gmi
	                         tilde.pink/documentation.gmi
	                         tilde.pink
	
	gmi100> 3                tilde.pink
	                         tilde.pink/documentation.gmi
	                         tilde.pink/docs/gemini.gmi
	                         tilde.pink/documentation.gmi
	                         tilde.pink
	                    >>>  gemini.circumlunar.space/


Devlog
------

### 2023.07.11 Initial motivation and thoughts

Authors of Gemini protocol claims that it should be possible to write
Gemini client in modern language [in less than 100 lines of code][1].
There are few projects that do that in programming languages with
garbage collectors, build in dynamic data structures and useful std
libraries for string manipulation, parsing URLs etc.

Intuition suggest that such achievement is not possible in plain C.
Even tho I decided to start this silly project and see how far I can
go with just ANSI C, std libraries and one dependency - OpenSSL.

It took me around 3 weeks of lazy slow programming to get to this
point but results exceeded my expectations.  It turned out that it's
not only achievable but also it's possible to include many convenient
features like persistent browsing history, links formatting, wrapping
of lines, pagination and some error handling.

My goal was to write in c89 standard avoiding any dirty tricks that
could buy me more lines like defining imports and constant values in
compiler command or writing multiple things in single line separated
with semicolon.  I think that final result can be called a normal C
code but OFC it is very dense, hard to read and uses practices that
are normally not recommended.  Even tho I call it a success.

I was not able to make better line wrapping work.  Ideally lines
should wrap at last whitespace that fits within defined boundary and
respects wide characters.  The best I could do in given constrains was
to do a hard line wrap after defined number of bytes.  Yes - bytes, so
it is possible to split wide character in half at the end of the line.
It can ruin ASCII art that uses non ASCII characters and sites written
mainly without ASCII characters.  This is the only thing that bothers
me.  Line wrapping itself is very necessary to make pagination and
pagination is necessary to make this program usable on terminals that
does not support scrolling.  Maybe it would be better to somehow
integrate gmi100 with pager like "less".  Then I don't have to
implement pagination and line wrapping at all.  That would be great.

I'm very happy that I was able to make browsing history work using
external file and not and array like in most small implementation I
have read.  With that this program is actually usable for me.  I'm
very happy about how the history works which is out of the ordinary
but I allows to have back and forward navigation with single logic.
With that I could fit 2 functionalities in single implementation.

I'm also very happy about links formatting.  Without this small
adjustment of output text I would not like to use this program for
actual browsing of Gemini space.

I thought about adding "default site" being the Gemini capsule that
opens by default when you run the program.  But that can be easily
done with small shell script or alias so I'm not going to do it.

```sh
echo "some.default.page.com" | gmi100
```

I's amazing how much can fit in 100 lines of C.

### 2023.07.12 - v2.0 the pager

Removing manual line wrapping and pagination in favor of pager program
that can be changed at any time was a great idea.  I love to navigate
Gemini holes with `cat` as pager when I'm in Emacs and with `less -X`
when in terminal.

### 2023.07.12 Wed 19:48 - v2.1 SSL issues and other changes

After using gmi100 for some time I noticed that often you stumble upon
a capsule by navigating directly to some distant path pointing at some
gemlog entry.  But then you want to visit home page of this author.
With current setup you would had to type URL by hand if visited page
did not provided handy "Go home" link.  Then I recalled that many GUI
browsers include "Up" and "Go home" buttons because you are able to
easily modify current URI to achieve such navigation.  This was
trivial to add in gmi100.  Required only single line that appends
`../` to current URI.  I added only "Up" functionality as navigation
to "Home" can be achieved by using "Up" few times in row and I don't
want to loose precious lines of code.

More than that, I changed default pager to `less` as it provides the
best experience in terminal and this is what people will use most of
the time including me.  For special cases in Emacs I can change pager
to `cat` with ease anyway.

Back to the main topic.  I had troubles opening many pages from
specific domains.  All of those probably run on the same server.  Some
kind o SSL error, not very specific.  I was able to open those pages
with this simple line of code:

```sh
$ openssl s_client -crlf -ign_eof -quiet -connect senders.io:1965 <<< "gemini://senders.io:1965/gemlog/"
```

Which means that servers work fine and there is something wrong in my
code.  I'm probably missing some SSL setting.

### 2023.07.13 Thu 04:56 - `SSL_ERROR_SSL` error fixed

I finally found it.  I had to use `SSL_set_tlsext_host_name` before
establishing connection.  I would not be able to figured it out by
myself.  All thanks to source code of project [gplaces][2].  And yes,
it's 5 am.

### 2023.07.18 Tue 16:42 - v3.0 I am complete! \m/

In v3 I completely redesigned core memory handling by switching to
files only.  With that program is now able to handle non text capsules
that contains images, music, videos and other.

In simpler words, server response body is always stored as temporary
file.  This file is then passed to pager program if MIME type is of
text type.  Else nothing happens but you can invoke any command on
this file so you can use `mpv` for media files or PDF viewer for
documents etc.  This also opens a lot of other possibilities.  For
example you can easily open currently loaded capsule in different
pager than default or in text editor or you can just use your system
default program with `xdg-open`.  And as log as you don't navigate to
other capsule you can keep using different commands on that file.

I also added few small useful commands like easy searching with `?`.
I was trying really hard to also implement handling for local files
with `file://` prefix.  But I would have to make links parser somehow
generic.  Right now it depends on SSL functions.  I don't see how to
fit that in current code structure.  I'm not planning any further
development.  I already achieved much more than I initially wanted.

I'm calling this project complete.

> I am complete!  
> Ha-aaaack  
> Yes, you are hacked  
> Git out of luck  
> Now I'm compiled  
> And my stack you will hack  
> This code will be mine  
> #include in first line  
> <you_brought_me_the_lib.h>  
> And now you shall compile  


[0]: https://www.openssl.org/
[1]: https://gemini.circumlunar.space/docs/faq.gmi
[2]: https://github.com/dimkr/gplaces/blob/gemini/gplaces.c#L841
