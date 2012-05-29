(9)
a = [12 34 54 656; 43 554 664 67];

fft(a)
(fft)(a) //Error: Unbalanced or unexpected parenthesis or bracket.

a = @(fft) //Error: Expression or statement is incomplete or incorrect.

b = @fft
b(a)

a= {790,'fdfsdj'}
(a){2} //Error: Unbalanced or unexpected parenthesis or bracket.
