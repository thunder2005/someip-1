/*******************************************************************************
* This file has been generated by Franca's FDeployGenerator.
* Source: deployment spec 'someip.ServiceSpecification'
*******************************************************************************/
package someip;

import org.franca.core.franca.FInterface;
import org.franca.core.franca.FMethod;
import org.franca.core.franca.FAttribute;
import org.franca.core.franca.FBroadcast;
import org.franca.deploymodel.core.FDeployedInterface;

/**
 * Accessor for deployment properties for 'someip.ServiceSpecification' specification
 */		
public class ServiceSpecificationInterfacePropertyAccessor
{
	
	private FDeployedInterface target;

	public ServiceSpecificationInterfacePropertyAccessor (FDeployedInterface target) {
		this.target = target;
	}
	
	public Integer getServiceID (FInterface obj) {
		return target.getInteger(obj, "ServiceID");
	}
	
	public Integer getMemberID (FMethod obj) {
		return target.getInteger(obj, "MemberID");
	}
	
	public Integer getMemberID (FBroadcast obj) {
		return target.getInteger(obj, "MemberID");
	}
	
	public Integer getValueChangedNotificationMemberID (FAttribute obj) {
		return target.getInteger(obj, "ValueChangedNotificationMemberID");
	}
	
	public Integer getSetterMemberID (FAttribute obj) {
		return target.getInteger(obj, "SetterMemberID");
	}
	
	public Integer getGetterMemberID (FAttribute obj) {
		return target.getInteger(obj, "GetterMemberID");
	}
	
	
}
