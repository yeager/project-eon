#include "data/native_code_image_admission.hpp"

#include "data/release_manifest.hpp"

#include <algorithm>
#include <array>

namespace eon {
namespace {
using B=NativeCodeAddressBasis;using L=NativeCodeLoadStatus;
constexpr std::array descriptors{
NativeCodeImageDescriptor{"e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123","millennium-dos-mill-com-linear","millennium-dos-launcher","4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e",0,1445,B::dos_com_linear_0x100,L::address_basis_declared},
NativeCodeImageDescriptor{"e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123","millennium-dos-titles-exe-linear","millennium-dos-title-flow","3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",0,7022,B::dos_com_linear_0x100,L::address_basis_declared},
NativeCodeImageDescriptor{"e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123","millennium-dos-2200ad-exe-linear","millennium-dos-game-flow","427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",0,54391,B::dos_com_linear_0x100,L::address_basis_declared},
NativeCodeImageDescriptor{"e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123","millennium-dos-2200gx-exe-linear","millennium-dos-gx-overlay","093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb",0,46634,B::dos_com_linear_0x100,L::address_basis_declared},
NativeCodeImageDescriptor{"2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400","millennium-amiga-defjam-shared-resident-linear","millennium-amiga-shared-resident","8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c",91136,180224,B::runtime_absolute,L::address_basis_declared},
NativeCodeImageDescriptor{"ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd","millennium-amiga-defjam-direct-bootstrap-linear","millennium-amiga-defjam-direct-bootstrap","8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c",1024,1024,B::runtime_absolute,L::address_basis_declared},
NativeCodeImageDescriptor{"ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd","millennium-amiga-defjam-direct-shared-resident-linear","millennium-amiga-direct-shared-resident","8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c",91136,180224,B::runtime_absolute,L::address_basis_declared},
NativeCodeImageDescriptor{"f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04","deuteros-amiga-clean-loaded-spans","deuteros-amiga-clean-main-stage","6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",22528,16896,B::runtime_absolute,L::address_basis_declared},
NativeCodeImageDescriptor{"f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04","deuteros-amiga-clean-loaded-spans","deuteros-amiga-clean-title-handoff","6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",450560,444928,B::runtime_absolute,L::address_basis_declared},
NativeCodeImageDescriptor{"c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653","deuteros-atari-replicants-first-stage-linear","deuteros-atari-replicants-first-stage","aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee",322560,4608,B::runtime_absolute,L::address_basis_declared},
NativeCodeImageDescriptor{"c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653","deuteros-atari-replicants-second-stage-linear","deuteros-atari-replicants-second-stage","aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee",18432,4608,B::runtime_absolute,L::address_basis_declared},
NativeCodeImageDescriptor{"c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653","deuteros-atari-killer-boot-linear","deuteros-atari-killer-boot","5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193",0,512,B::disk_relative,L::unproven},
NativeCodeImageDescriptor{"0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69","millennium-atari-equinox-direct-boot-disk-relative-linear","millennium-atari-equinox-direct-bootstrap","3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7",0,512,B::disk_relative,L::unproven},
};
}

std::span<const NativeCodeImageDescriptor> native_code_image_manifest(){return descriptors;}

NativeCodeImageAdmissionResult admit_native_code_image(const VerifiedReleaseMedia& media,
    const std::string_view image_id,const std::string_view range_id){
    NativeCodeImageAdmissionResult result;
    const auto found=std::find_if(descriptors.begin(),descriptors.end(),[&](const auto& d){return d.release_sha256==media.release().sha256&&d.image_id==image_id&&d.range_id==range_id;});
    if(found==descriptors.end()){result.error="Code image/range is not mapped for this exact release";return result;}
    if(found->load_status==L::unproven||found->address_basis==B::disk_relative){result.error="Code image has byte coverage but no proven runtime load map";return result;}
    const auto profiles=parser_profile_manifest();const auto profile=std::find_if(profiles.begin(),profiles.end(),[&](const auto&p){return p.id==found->range_id&&p.release_sha256==found->release_sha256;});
    if(profile==profiles.end()||profile->leaf_sha256!=found->source_sha256||profile->offset!=found->source_offset||profile->length!=found->length){result.error="Code image descriptor is detached from parser manifest";return result;}
    if(!verified_release_media_has_declared_profile_ranges(media)){result.error="Release media parser ranges are not verified";return result;}
    const auto leaf=media.borrow(found->source_sha256);if(!leaf||found->source_offset>leaf->size()||found->length>leaf->size()-found->source_offset){result.error="Verified source leaf cannot provide the exact code range";return result;}
    result.view=NativeCodeImageView{*found,leaf->subspan(static_cast<std::size_t>(found->source_offset),static_cast<std::size_t>(found->length))};return result;
}
} // namespace eon
