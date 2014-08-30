import sys
import os

def main():
	file1 = sys.argv[1]
	file2='bhav'
	f = open(file1,'r').readlines()
	for i in range(1,12):
		print i
		g = open(file2,'w')
		f[0]=str(i)+'\n'
		man=""
		for line in f:
			man=man+line
		print>>g, man
		print "-----------------------ALGO ", i,"---------------------------------------------------------"
		os.system("./nachos -F "+file2+ " > outputbhav"+str(i))
		

if __name__=='__main__':
	main()
