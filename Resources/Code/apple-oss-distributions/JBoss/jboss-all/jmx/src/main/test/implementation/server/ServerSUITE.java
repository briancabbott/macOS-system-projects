/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.server;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * JBossMX Specific Server tests.
 *
 * @author <a href="mailto:AdrianBrock@HappeningTimes.com">Adrian Brock</a>.
 */
public class ServerSUITE
  extends TestSuite
{
  /**
   * Run the tests
   * 
   * @param args the arguments for the test
   */
  public static void main(String[] args)
  {
    junit.textui.TestRunner.run(suite());
  }

  /**
   * Get a list of tests.
   *
   * @return the tests
   */
  public static Test suite()
  {
    TestSuite suite = new TestSuite("JBossMX Specific Server tests");

    suite.addTest(new TestSuite(ObjectInstanceTestCase.class));

    return suite;
  }
}
