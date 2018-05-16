main: mftp mftpserve

mftp: mftp.c mftp.h
	gcc -g mftp.c -o mftp

mftpserve: mftpserve.c mftp.h
	gcc -g mftpserve.c -o mftpserve

clean:
		rm -f mftpserve mftpserve.o mftp mftp.o