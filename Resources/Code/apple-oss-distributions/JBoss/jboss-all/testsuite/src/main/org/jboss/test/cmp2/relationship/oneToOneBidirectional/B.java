package org.jboss.test.cmp2.relationship.oneToOneBidirectional;

import javax.ejb.EJBLocalObject;

public interface B extends EJBLocalObject {
	public A getA();
	public void setA(A a);
}
