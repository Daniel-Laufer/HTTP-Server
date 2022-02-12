mil = 1000000
n = 2 * mil
for i in range(1,10):
    n = (2**i) * mil
    with open(f"358_files/{n//mil}MB.txt", 'w') as f:
        for i in range(n):
            f.write('a')





    
