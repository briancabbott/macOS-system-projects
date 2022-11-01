! Test character variables in data statements
! Also substrings of cahracter variables.
! PR14976 PR16228 
program data_char_1
  character(len=5) :: a(2)
  character(len=5) :: b(2)
  data a /'Hellow', 'orld'/
  data b(:)(1:4), b(1)(5:5), b(2)(5:5) /'abcdefg', 'hi', 'j', 'k'/
  
  if ((a(1) .ne. 'Hello') .or. (a(2) .ne. 'orld ')) call abort
  if ((b(1) .ne. 'adcdl') .or. (b(2) .ne. 'hi  l')) call abort
end program
