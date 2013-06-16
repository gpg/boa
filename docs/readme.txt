
   [IMAGE]
   
                                      BOA
                                       
   Welcome to your personal documentation for Boa, a high performance web
   server for Unix-alike computers, covered by the Gnu General Public
   License. The current release is boa-0.92. The on-line, updated copy
   of this documentation lives at www.boa.org.
   
   Boa is a single-tasking HTTP server. That means that unlike
   traditional web servers, it does not fork for each incoming
   connection, nor does it fork many copies of itself to handle multiple
   connections. It internally multiplexes all of the ongoing HTTP
   connections, and forks only for CGI programs (which must be separate
   processes.) Preliminary tests show boa is about twice as fast as
   Apache, and is capable of handling 50 hits per second on a 66 MHz
   '486.
   
   The primary design goals of Boa are speed and security. Security, in
   the sense of "can't be subverted by a malicious user", not "fine
   grained access control and encrypted communications". Boa is not
   intended as a feature-packed server; if you want one of those, check
   out WN from John Franks. Modifications to Boa that improve its speed,
   security, robustness, and portability, are eagerly sought. Other
   features may be added if they can be achieved without hurting the
   primary goals.
   
   Boa was created by Paul Phillips <psp@well.com>. It is now being
   maintained and enhanced by Larry Doolittle <ldoolitt@cebaf.gov>. Other
   possible contact people are Charles F. Randall (lost), and Jon Nelson
   <nels0988@tc.umn.edu>. Linux is the development platform at the
   moment, other OS's are known to work. If you'd like to contribute to
   this effort, contact Larry or Jon via e-mail.
   
More information:

     * Installation and Usage
     * Performance limits and design philosophy
     * Differences between Boa and other web servers
     * Possible bugs
     * Possible unexpected behavior
     * Possible improvements
     * License
     * Acknowledgments
     * Logos
     * Bibliography
     * (historical) Paul Phillips' README for release 0.91
       
   
     _________________________________________________________________
   
   Last update: 20 December, 1996
   Larry Doolittle
