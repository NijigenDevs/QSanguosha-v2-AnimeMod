sgs.ai_skill_invoke.huanxing = function(self, data)
	local use = data:toCardUse()
	if not use or not use.from then return false end
	if self:isEnemy(use.from) then return true end
	return false
end

sgs.ai_skill_invoke.fushang = function(self, data)
	local damage = data:toDamage()
	if not damage or not damage.to then return false end
	if self:isFriend(damage.to) then
		local num = damage.to:getMark("@fushang")
		local last = damage.to:getMark("@fushang_time")
		if last == 1 then
			if self:isWeak(damage.to) and self:getCardsNum("Peach") == 0 then return false end
			if num >= damage.to:getLostHp() - 1 then return false end
			return true
		else
			return true
		end
	end
	return false
end

--skill guangyu
sgs.ai_skill_use["@@guangyu"] = function(self)
	local ids = self.player:property("guangyu"):toString():split("+")
	for _, id in ipairs(ids) do
		local card = sgs.Sanguosha:getCard(id)
		if self.player:isCardLimited(card, sgs.Card_MethodUse) then continue end
		local card_str = ("key_trick:guangyu[%s:%s]=%d"):format(card:getSuitString(), card:getNumberString(), id)
		local ss = sgs.Card_Parse(card_str)
		local dummy_use = { isDummy = true , to = sgs.SPlayerList() }
		self:useCardKeyTrick(ss, dummy_use)
		if dummy_use.card and not dummy_use.to:isEmpty() then
			return card_str .. "->" .. dummy_use.to:first():objectName()
		end
	end
	return "."
end
sgs.ai_skill_invoke.guangyu = function(self, data)
	if self:isFriend(self.room:getCurrent()) then return true end
	return false
end

--skill xiyuan
sgs.ai_skill_invoke.xiyuan = function(self, data)
	if #self.friends_noself == 0 then return false end
	return true
end

sgs.ai_skill_playerchosen.xiyuan = function(self, targets)
	for _,p in ipairs(self.friends_noself) do
		if not p:getGeneral2() and self:isWeak(p) then return p end
		if p:hasSkill("se_diangong") then return p end
	end
	return self.friends_noself[1]
end

--skill dingxin
sgs.ai_skill_choice["dingxin"] = function(self, choices, data)
	if self:getCardsNum("Peach") == 0 then return "dingxin_recover" end
	for _,p in sgs.qlist(room:getPlayers()) do
		if (p:getGeneralName() == "Nagisa" or p:getGeneral2Name() == "Nagisa") and (self.player:getRole() == p:getRole() or (self.player:getRole() == "lord" and p:getRole() == "loyalist")) then return "dingxin_revive" end
	end
	return "dingxin_recover"
end



--麻婆豆腐
function SmartAI:useCardMapoTofu(card, use) --need help 这个锦囊太复杂了。。。。。
	local targets = {}
	for _,p in sgs.qlist(self.room:getAlivePlayers()) do
		if self.player:distanceTo(p) <= 1 then table.insert(targets, p) end
	end
	if #targets == 0 then return end
	local f_target
	for _,target in ipairs(targets) do
		if self:isFriend(target) then
			if target:hasSkills(sgs.masochism_by_self_skill) then f_target = target end
			if target:hasSkills("Tianhuo") and target:getLostHp() > 0 then return target end
			local touma = self.room:findPlayerBySkillName("Huansha")
			local shirayuki = self.room:findPlayerBySkillName("SE_Zhandan")
			if ((touma and self:isFriend(touma)) or (shirayuki and self:isFriend(shirayuki))) and target:getLostHp() > 0 then return target end
		else
			if self.player:hasSkills(sgs.weak_killer_skill) and target:getLostHp() == 0 then f_target = target end
		end
	end
	if not f_target then
		for _,target in ipairs(targets) do
			if self:isFriend(target) and target:getLostHp() > 0 then
				f_target = target
			end
		end
	end
	if self.player:hasSkill("se_shouren") then
		for _,target in ipairs(targets) do
			if self:isEnemy(target) then
				f_target = target
			end
		end
	end
	if f_target then
		for _,v in ipairs(self.enemies) do
			if v:objectName() ~= f_target:objectName() then
				use.card = card
				if use.to and not (self.room:isProhibited(self.player, v, card) or self.room:isAkarin(self.player, v)) then use.to:append(v) end
				return
			end
		end
	end
end
sgs.ai_use_priority.MapoTofu = 10
sgs.ai_use_value.MapoTofu = 8
sgs.ai_keep_value.MapoTofu = 1.0
sgs.ai_card_intention.MapoTofu = 0

--KEY

function SmartAI:useCardKeyTrick(card, use)
	for _,v in ipairs(self.friends) do
		if v:getLostHp() > 0 and not v:containsTrick("key_trick") and not (self.room:isProhibited(self.player, v, card) or self.room:isAkarin(self.player, v)) then
			use.card = card
			if use.to then use.to:append(v) end
			return
		end
	end
	for _,v in ipairs(self.friends) do
		if not v:containsTrick("key_trick") and not (self.room:isProhibited(self.player, v, card) or self.room:isAkarin(self.player, v)) then
			use.card = card
			if use.to then use.to:append(v) end
			return
		end
	end
end
sgs.ai_use_priority.KeyTrick = 3
sgs.ai_use_value.KeyTrick = 2
sgs.ai_keep_value.KeyTrick = 2
sgs.ai_card_intention.KeyTrick = -50

--shengjian_black
sgs.ai_skill_playerchosen.shengjian_black = function(self, targets)
	local source = self.player
	local power = 0
	for _,enemy in ipairs(self.enemies) do
		local force = math.abs(source:getEquips():length() - enemy:getEquips():length()) + (enemy:getEquips():length())/2
		if force > power then
			target = enemy
			power = force
		end
	end
	return target
end
	

--冈崎朋也
se_zhuren_skill={}
se_zhuren_skill.name="zhuren"
table.insert(sgs.ai_skills,se_zhuren_skill)
se_zhuren_skill.getTurnUseCard=function(self,inclusive)
	if #self.friends <= 1 then return end
	local source = self.player
	if source:isKongcheng() then return end
	if source:hasUsed("ZhurenCard") then return end
	return sgs.Card_Parse("@ZhurenCard:.:")
end

sgs.ai_skill_use_func.ZhurenCard = function(card,use,self)
	local target
	local source = self.player
	local max_num = source:getMaxHp() - source:getHp() + 1
	local max_x = 0
	for _,friend in ipairs(self.friends) do
		local x = 5 - friend:getHandcardNum()

		if x > max_x and friend:objectName() ~= source:objectName() then
			max_x = x
			target = friend
		end
	end
	local cards=sgs.QList2Table(self.player:getHandcards())
	local needed = {}
	for _,acard in ipairs(cards) do
		if #needed < max_num then
			table.insert(needed, acard:getEffectiveId())
		end
	end
	if target and needed then
		use.card = sgs.Card_Parse("@ZhurenCard:"..table.concat(needed,"+")..":")
		if use.to then use.to:append(target) end
		return
	end
end

sgs.ai_use_value.ZhurenCard = 4
sgs.ai_use_priority.ZhurenCard  = 2.4
sgs.ai_card_intention.ZhurenCard  = -60

sgs.ai_skill_choice.Daolu = function(self, data)
	local lord = self.room:getLord()
	if self.player:getRole() == "lord" then
		for _,friend in ipairs(self.friends) do
			if self:isWeak(friend) and friend:objectName() ~= self.player:objectName() then
				return "Fuko_summoner"
			end
		end
		return "Nagisa_Protector"
	elseif self.player:getRole() == "loyalist" then
		return "Tomoyo_Couple"
	elseif self.player:getRole() == "rebel" then
		for _,friend in ipairs(self.friends) do
			if self:isWeak(friend) and self.player:getHp() > 2 and friend:objectName() ~= self.player:objectName() then
				if #self.friends > #self.enemies then
					return "Fuko_summoner"
				end
			end
			--TODO
		end
		return "Nagisa_Protector"
	elseif self.player:getRole() == "renegade" then
		if lord:getHp() <= 2 then
			return "Fuko_summoner"
		end
		return "Nagisa_Protector"
	end
end

sgs.ai_skill_playerchosen.Daolu = function(self, targets)
	local lord = self.room:getLord()
	if self.player:getRole() == "loyalist" then return lord end
	if self.player:getRole() == "rebel" then
		for _,friend in ipairs(self.friends) do
			if not friend:objectName()~=self.player:objectName() and self:isWeak(friend) and self.player:getHp() > 2 then
				target = friend
			end
		end
	end
	if target then return target end
	for _,friend in ipairs(self.friends) do
		if not friend:objectName()~=self.player:objectName() then
			target = friend
		end
	end
	return target
end

sgs.ai_playerchosen_intention.Daolu = function(from, to)
	local intention = -100
	sgs.updateIntention(from, to, intention)
end

local se_diangong_skill={}
se_diangong_skill.name="diangong"
table.insert(sgs.ai_skills,se_diangong_skill)
se_diangong_skill.getTurnUseCard=function(self,inclusive)
	local cards = sgs.QList2Table(self.player:getHandcards()) 
	self:sortByUseValue(cards,true)  
	for _,enemy in ipairs(self.enemies) do
		if enemy:hasSkills("se_qidian|suipian|guangyu") then return end
	end
	for _,acard in ipairs(cards) do
		if self:getKeepValue(acard)<5 and acard:isBlack() then    
			local number = acard:getNumberString()
			local card_id = acard:getEffectiveId()
			local suit = acard:getSuitString()
			return sgs.Card_Parse("@DiangongCard:.:")
		end
	end
end

sgs.ai_skill_use_func.DiangongCard = function(card,use,self)
	local target
	for _,enemy in sgs.qlist(room:getAlivePlayers()) do
		local pl = true
		for _,card in sgs.qlist(enemy:getJudgingArea()) do
			if card:isKindOf("Lightning") then
				pl = false
			end
		end
		if pl then
			target = pl
		end
	end
	local needed
	for _,acard in ipairs(cards) do
		if self:getKeepValue(acard)<5 and acard:isBlack() then    
			needed = acard
		end
	end
	if target and needed then
		use.card = sgs.Card_Parse("@DiangongCard:"..needed:getEffectiveId()..":")
		if use.to then use.to:append(target) end
		return
	end
end

sgs.ai_skill_invoke.huanxing = function(self, data)
	local use = data:toCardUse()
	if not use or not use.from then return false end
	if self:isEnemy(use.from) then return true end
	return false
end

sgs.ai_use_value.DiangongCard = 8
sgs.ai_use_priority.DiangongCard = 2.5


sgs.ai_skill_invoke.haixing = function(self, data)
	local dying_data = data:toDying()
	local source = dying_data.who
	local mygod= self.room:findPlayerBySkillName("haixing")
	if self:isFriend(mygod) then
		return true
	end
	return false
end