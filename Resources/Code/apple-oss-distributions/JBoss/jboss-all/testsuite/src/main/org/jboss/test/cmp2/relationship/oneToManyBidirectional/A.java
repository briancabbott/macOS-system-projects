package org.jboss.test.cmp2.relationship.oneToManyBidirectional;

import java.util.Collection;
import javax.ejb.EJBLocalObject;

public interface A extends EJBLocalObject {
	public Collection getB();
	public void setB(Collection b);
}
