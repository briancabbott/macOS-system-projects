/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

/**
 * This exception is thrown when the EJB-QL parser encounters an unknown path.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1.4.1 $
 */                            
public final class UnknownPathException extends RuntimeException {
   private final String reason;
   private final String path;
   private final String fieldName;
   private final int errorLine;
   private final int errorColumn;
   
   public UnknownPathException(
         String reason,
         String path,
         String fieldName,
         int errorLine,
         int errorColumn) {

      super(reason + ": at line " + errorLine + ", "+
            "column " + errorColumn + ".  " +
            "Encountered: \"" + fieldName + "\"" +
            ((path==null) ? "" : " after: \"" + path + "\"") );

      this.reason = reason;
      this.path = path;
      this.fieldName = fieldName;
      this.errorLine = errorLine;
      this.errorColumn = errorColumn;
   }
   public String getReason() {
      return reason;
   }
   public String getCurrentPath() {
      return path;
   }
   public String getFieldName() {
      return fieldName;
   }
   public int getErrorLine() {
      return errorLine;
   }
   public int getErrorColumn() {
      return errorColumn;
   }
}
