a= {790,'fdfsdj'}
a{2}(3)
a{2}(4)
a{2}()
a{2}(:)
[a{2}(:)]
b = [a{2}(:); 91; 92 ]

a = [12 34 54 656; 43 554 664 67];
c= [fft(a), [2;1]]

b = [@sin,@cos] //??? Error using ==> horzcat
//Nonscalar arrays of function handles are not allowed; use cell arrays
//instead.

trigFun = {@sin, @cos, @tan};
trigFun{2}(-pi:0.01:pi)

