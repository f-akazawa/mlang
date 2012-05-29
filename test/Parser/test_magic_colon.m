% Read columns 1-5 of all rows
A = rand(5,10);
B = A(:, 1:5);


% Convert a matrix or array to a column vector using the colon
% operator as a single index
A = rand(3,4);
B = A(:);

% Using the colon operator on the left side of an assignment 
% statement, you can assign new values to array elements without
% changing the shape of the array
A = rand(3,4);
A(:) = 1:12;

