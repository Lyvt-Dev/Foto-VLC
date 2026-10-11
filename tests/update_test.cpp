#include "update_core.hpp"
#include <iostream>
int main(){
    int passed=0,failed=0;auto check=[&](bool value,const char* name){std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n';value?++passed:++failed;};
    check(paf::newer("0.7.2-fix","0.7.2"),"Fix release upgrades base version");
    check(!paf::newer("0.7.2","0.7.2-fix"),"Base release cannot downgrade fix");
    check(paf::newer("0.7.3","0.7.2-fix"),"Next numeric release upgrades fix");
    check(!paf::newer("0.7.2-fix","0.7.2-fix"),"No reinstall of identical fix");
    check(paf::newer("0.10.0","0.9.99"),"Numeric version ordering");
    check(!paf::newer("0.5.0","0.5.0"),"No reinstall of equal version");
    check(!paf::newer("0.4.0","0.5.0"),"No automatic downgrade");
    for(const char* text:{"01.2.3","1.2","1.2.3-beta","1.2.3/../../setup","65536.0.0","1.-1.2","1.2.3.4","1.2.3\n"})check(!paf::version(text),"Reject malformed or unsafe version");
    const std::string digest(64,'a');std::string record="{\"version\":\"0.5.0\",\"sha256\":\""+digest+"\",\"size\":12492498}";
    auto fixed=record;fixed.replace(fixed.find("0.5.0"),5,"0.7.2-fix");check(paf::manifest(fixed).has_value(),"Parse fixed-release manifest");
    auto value=paf::manifest(record);check(value&&value->version=="0.5.0"&&value->size==12492498,"Parse release manifest");
    check(!paf::manifest(record+"junk"),"Reject trailing manifest data");
    check(!paf::manifest(std::string(9000,' ')),"Bound manifest memory");
    check(!paf::manifest("{\"version\":\"0.5.0\",\"sha256\":\"bad\",\"size\":12492498}"),"Reject invalid checksum");
    check(!paf::manifest("{\"version\":\"0.5.0\",\"sha256\":\""+digest+"\",\"size\":999999999}"),"Bound installer size");
    check(!paf::manifest("{\"version\":\"0.5.0\",\"sha256\":\""+digest+"\",\"size\":1}"),"Reject empty installer");
    check(!paf::manifest("{\"version\":\"0.5.0\",\"sha256\":\""+digest+"\",\"size\":12492498,\"url\":\"http://example.test/evil.exe\"}"),"Manifest cannot redirect executable source");
    std::cout<<passed<<" checks passed\n";return failed?1:0;
}
