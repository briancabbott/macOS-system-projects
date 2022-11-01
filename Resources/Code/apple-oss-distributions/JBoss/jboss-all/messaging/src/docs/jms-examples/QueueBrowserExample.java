/*
 * @(#)SynchQueueReceiver.java	1.7 00/08/14
 * 
 * Copyright (c) 2000 Sun Microsystems, Inc. All Rights Reserved.
 * 
 * Sun grants you ("Licensee") a non-exclusive, royalty free, license to use,
 * modify and redistribute this software in source and binary code form,
 * provided that i) this copyright notice and license appear on all copies of
 * the software; and ii) Licensee does not utilize the software in a manner
 * which is disparaging to Sun.
 *
 * This software is provided "AS IS," without a warranty of any kind. ALL
 * EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NON-INFRINGEMENT, ARE HEREBY EXCLUDED. SUN AND ITS LICENSORS SHALL NOT BE
 * LIABLE FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THE SOFTWARE OR ITS DERIVATIVES. IN NO EVENT WILL SUN OR ITS
 * LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA, OR FOR DIRECT,
 * INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR PUNITIVE DAMAGES, HOWEVER
 * CAUSED AND REGARDLESS OF THE THEORY OF LIABILITY, ARISING OUT OF THE USE OF
 * OR INABILITY TO USE SOFTWARE, EVEN IF SUN HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * This software is not designed or intended for use in on-line control of
 * aircraft, air traffic, aircraft navigation or aircraft communications; or in
 * the design, construction, operation or maintenance of any nuclear
 * facility. Licensee represents and warrants that it will not use or
 * redistribute the Software for such purposes.
 */
import javax.jms.*;

/**
 * The QueueBrowser class consists only of a main method, which 
 * browses one or more messages from a queue.  Run 
 * this program in conjunction with SenderToQueue.  Specify a queue name on the
 * command line when you run the program.
 * <p>
 * The program calls methods in the SampleUtilities class.
 *
 * @author Kim Haase
 * @author Hiram Chirino (Cojonudo14@hotmail.com)
 */
public class QueueBrowserExample {

	/**
	 * Main method.
	 *
	 * @param args	the queue used by the example
	 */
	public static void main(String[] args) {
		String                  queueName = null;
		QueueConnectionFactory  queueConnectionFactory = null;
		QueueConnection         queueConnection = null;
		QueueSession            queueSession = null;
		Queue                   queue = null;
		QueueBrowser            queueBrowser = null;
		TextMessage             message = null;
		int                     exitResult = 0;
				
		/*
		 * Read queue name from command line and display it.
		 */
		if (args.length != 1) {
		    System.out.println("Usage: java QueueBrowserExample <queue_name>");
		    System.exit(1);
		}
		queueName = new String(args[0]);
		System.out.println("Queue name is " + queueName);
		    
		/*
		 * Obtain connection factory.
		 * Create connection.
		 * Create session from connection; false means session is not
		 * transacted.
		 * Obtain queue name.
		 */
		try {
		    queueConnectionFactory = 
		        SampleUtilities.getQueueConnectionFactory();
		    queueConnection = 
		        queueConnectionFactory.createQueueConnection();
			queueSession = queueConnection.createQueueSession(false, 
				Session.AUTO_ACKNOWLEDGE);
			queue = SampleUtilities.getQueue(queueName, queueSession);
		} catch (Exception e) {
			System.out.println("Connection problem: " + e.toString());
			if (queueConnection != null) {
				try {
					queueConnection.close();
				} catch (JMSException ee) {}
			}
		    System.exit(1);
		} 
		
		try {
			
			queueBrowser = queueSession.createBrowser(queue);
			queueConnection.start();

			java.util.Enumeration enum = queueBrowser.getEnumeration();
			while( enum.hasMoreElements() ) {
				Message m = (Message)enum.nextElement();
			    System.out.println("Browsed Message: " + m);
			}				

		} catch (Throwable e) {
			System.out.println("Exception occurred: " + e.toString());
			e.printStackTrace();
			exitResult = 1;
		} finally {
			if (queueConnection != null) {
				try {
					queueConnection.close();
				} catch (JMSException e) {
					exitResult = 1;
				}
			}
		}   	     
		SampleUtilities.exit(exitResult);
	}
}
