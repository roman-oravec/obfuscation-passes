import random
import sys

a = random.randrange(1,2**32-1,2) # a needs to be odd
b = random.randrange(1,2**32-1)
a_inv = pow(a, -1, 2**32)

# Generate two random 32b numbers, a being odd
# f = g^-1
# E is an MBA expression non-trivially equal to 0
# e.g. (x^y)-2*(x&y) == x + y --> E = x+y-(x^y)-2*(x&y)
# K is a constant to hide
# K = g(E + f(K))
def mba(k):
    x = random.randrange(1,2**32-1)
    y = random.randrange(1,2**32-1)
    fk = f(k)
    E = x+y-(x^y)-2*(x&y)
    print("x =", x, "\ny =",y, "\nf(k)=", fk)
    obf = g((E+fk))
    print(k ,"==", obf)
    return obf

def f(x):
    return (a*x+b) % 2**32

def g(x):
    return (a_inv*x+(-a_inv*b)) % 2**32

# First version, just use mba()
def compose(x):
    fx = (a*x+b) % 2**32
    gfx = (a_inv*fx+(-a_inv*b)) % 2**32
    print("g(f(x)) =", a_inv, "*", fx, "+", "(-", a_inv,"*",b,"))", "mod 2^32 =", gfx)
    return

x = int(sys.argv[1])
print("x =",x)
compose(x)
mba(x)


